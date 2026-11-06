//go:build e2e

package acceptance_test

import (
	"database/sql"
	"encoding/binary"
	"net"
	"os"
	"strconv"
	"strings"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/client"
	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
	_ "github.com/go-sql-driver/mysql"
)

type actor struct {
	guid    uint64
	name    string
	race    uint8
	class   uint8
	level   int
	profile uint8
}

const chatWhisper = 0x07

func openDatabase(t *testing.T, key string) *sql.DB {
	t.Helper()
	dsn := os.Getenv(key)
	if dsn == "" {
		t.Fatalf("Required isolated-realm connection %s is missing", key)
	}
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { db.Close() })
	if err = db.Ping(); err != nil {
		t.Fatalf("Cannot connect to %s: %v", key, err)
	}
	return db
}

func questingActor(t *testing.T, chars, advai *sql.DB) actor {
	t.Helper()
	deadline := time.NewTimer(2 * time.Minute)
	defer deadline.Stop()
	tick := time.NewTicker(time.Second)
	defer tick.Stop()
	for {
		rows, err := advai.Query("SELECT guid, state FROM playerbots_advai_state ORDER BY guid")
		if err != nil {
			t.Fatal(err)
		}
		var found actor
		for rows.Next() {
			var guid uint64
			var state string
			if err = rows.Scan(&guid, &state); err != nil {
				rows.Close()
				t.Fatal(err)
			}
			fields := strings.Fields(state)
			if len(fields) < 10 {
				rows.Close()
				t.Fatalf("Invalid ADVAI state header for %d", guid)
			}
			switch fields[0] {
			case "2", "3", "4", "5", "6", "7", "8", "9", "10": // Shared lifecycle and goal header; later fields are versioned.
			default:
				rows.Close()
				t.Fatalf("Unsupported ADVAI state version %q for %d", fields[0], guid)
			}
			if fields[1] != "1" || fields[5] != "0" || fields[9] != "7" {
				continue
			}
			var candidate actor
			candidate.guid = guid
			err = chars.QueryRow(`SELECT name, race, class, level FROM characters c
                WHERE guid=? AND online=1 AND map IN (0,1,530,571)
                AND NOT EXISTS (SELECT 1 FROM group_member g WHERE g.memberGuid=c.guid)`, guid).
				Scan(&candidate.name, &candidate.race, &candidate.class, &candidate.level)
			if err == sql.ErrNoRows {
				continue
			}
			if err != nil {
				rows.Close()
				t.Fatal(err)
			}
			found = candidate
			if fields[0] == "6" || fields[0] == "7" || fields[0] == "8" || fields[0] == "9" || fields[0] == "10" {
				profileIndex := len(fields) - 1
				if fields[0] == "7" {
					profileIndex -= 2
				}
				if fields[0] == "8" || fields[0] == "9" || fields[0] == "10" {
					profileIndex -= 3
				}
				profile, parseErr := strconv.ParseUint(fields[profileIndex], 10, 8)
				if parseErr != nil || profile > 2 {
					rows.Close()
					t.Fatalf("Invalid class profile for %d", guid)
				}
				found.profile = uint8(profile)
			}
			break
		}
		rowErr := rows.Err()
		rows.Close()
		if rowErr != nil {
			t.Fatal(rowErr)
		}
		if found.guid != 0 {
			return found
		}
		select {
		case <-deadline.C:
			t.Fatal("Precondition failed: no initialized, online, ungrouped questing ADVAI character")
		case <-tick.C:
		}
	}
}

func alliance(race uint8) bool {
	return race == 1 || race == 3 || race == 4 || race == 7 || race == 11 || race == 12
}

func whisper(t *testing.T, human *e2eharness.ScenarioBot, target actor, message string) {
	t.Helper()
	language := uint32(1)
	if alliance(target.race) {
		language = 7
	}
	// Whisper CMSG_MESSAGECHAT has recipient and message C strings after type and language.
	if err := human.World.SendChatMessage(chatWhisper, language, target.name+"\x00"+message); err != nil {
		t.Fatal(err)
	}
}

func armWhisper(human *e2eharness.ScenarioBot, sender uint64) (<-chan string, func()) {
	lines := make(chan string, 8)
	cancel := human.World.AddPacketHook(func(opcode uint16, data []byte) {
		if opcode != client.SmsgMessageChat || len(data) < 29 || data[0] != chatWhisper ||
			binary.LittleEndian.Uint64(data[5:13]) != sender {
			return
		}
		length := int(binary.LittleEndian.Uint32(data[25:29]))
		if length < 1 || length > len(data)-29 {
			return
		}
		text := strings.TrimRight(string(data[29:29+length]), "\x00")
		select {
		case lines <- text:
		default:
		}
	})
	return lines, cancel
}

// Checks the actual player protocol, a factual response, and invitation consent against a live realm.
func TestConversationFactsAndGroupConsent(t *testing.T) {
	chars := openDatabase(t, "E2E_CHAR_DSN")
	advai := openDatabase(t, "E2E_PLAYERBOTS_DSN")
	target := questingActor(t, chars, advai)

	t.Logf("Selected ADVAI %s (%d), class %d, level %d", target.name, target.guid, target.class, target.level)
	race := uint8(2)
	if alliance(target.race) {
		race = 1
	}
	human := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{
		Prefix: "AdvChat", Race: race, Class: 1, Level: target.level,
	})
	t.Cleanup(func() {
		if human.InGroup() {
			human.LeaveGroup(t)
			human.WaitNotInGroup(t, 10*time.Second)
		}
	})
	lines, cancel := armWhisper(human, target.guid)
	defer cancel()
	whisper(t, human, target, "what class are you?")
	classes := map[uint8]string{1: "warrior", 2: "paladin", 3: "hunter", 4: "rogue", 5: "priest",
		6: "death knight", 7: "shaman", 8: "mage", 9: "warlock", 11: "druid"}
	expected, exists := classes[target.class]
	if target.profile == 1 {
		expected = "cultist"
	} else if target.profile == 2 {
		expected = "necromancer"
	}
	if !exists {
		t.Fatalf("Unsupported character class in fixture: %d", target.class)
	}
	select {
	case text := <-lines:
		t.Logf("ADVAI transcript human=%d bot=%d: %s", human.GUID, target.guid, text)
		if strings.Contains(text, "%facts") || !strings.Contains(strings.ToLower(text), expected) {
			t.Fatalf("Response does not match actual class %s: %s", expected, text)
		}
	case <-time.After(100 * time.Second):
		t.Fatal("ADVAI delivered no class response within the conversation lifetime")
	}
	waitInvite, cancelInvite := human.ArmGroupInvite()
	defer cancelInvite()
	whisper(t, human, target, "can you invite me to quest together?")
	inviter, received := waitInvite(30 * time.Second)
	if !received || !strings.EqualFold(inviter, target.name) {
		t.Fatalf("Expected an actual invitation from %s, got %q (received=%v)", target.name, inviter, received)
	}
	if human.InGroup() {
		t.Fatal("Human was added to a group before accepting the invitation")
	}
	human.AcceptGroup(t)
	state := human.WaitGroupList(t, true, 2, 15*time.Second)
	t.Logf("PASS: truthful response and consent-based group join with ADVAI %s (%d): %+v", target.name, target.guid, state)
}

func TestZZShutdownIsolatedRealm(t *testing.T) {
	if os.Getenv("E2E_SHUTDOWN_REALM") != "1" {
		t.Skip("Explicit isolated-realm shutdown was not requested")
	}
	if e2eharness.AuthAddr != "127.0.0.1:13724" || !strings.Contains(e2eharness.CharDSN, "/advai_characters") {
		t.Fatal("Shutdown is restricted to the isolated ADVAI realm")
	}
	human := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "AdvStop", Race: 1, Class: 1, Level: 1})
	human.GM(t, ".server shutdown 3")
	deadline := time.NewTimer(30 * time.Second)
	defer deadline.Stop()
	tick := time.NewTicker(100 * time.Millisecond)
	defer tick.Stop()
	for {
		connection, err := net.DialTimeout("tcp", "127.0.0.1:18085", 200*time.Millisecond)
		if err != nil {
			return
		}
		connection.Close()
		select {
		case <-deadline.C:
			t.Fatal("Server did not acknowledge the shutdown by closing its listener")
		case <-tick.C:
		}
	}
}
