//go:build e2e

package acceptance_test

import (
	"encoding/binary"
	"testing"
	"time"

	"github.com/azerothcore/AzerothGhost/e2e/e2eharness"
)

func awaitState(t *testing.T, label string, timeout time.Duration, condition func() bool) {
	t.Helper()
	deadline := time.NewTimer(timeout)
	defer deadline.Stop()
	tick := time.NewTicker(25 * time.Millisecond)
	defer tick.Stop()
	for !condition() {
		select {
		case <-deadline.C:
			t.Fatalf("Timed out observing %s", label)
		case <-tick.C:
		}
	}
}

func trainerPurchase(t *testing.T, human *e2eharness.ScenarioBot, trainer uint64, spell uint32, success bool) {
	t.Helper()
	results := make(chan bool, 1)
	cancel := human.World.AddPacketHook(func(opcode uint16, data []byte) {
		if (opcode != 0x1b3 && opcode != 0x1b4) || len(data) < 12 ||
			binary.LittleEndian.Uint64(data[:8]) != trainer || binary.LittleEndian.Uint32(data[8:12]) != spell {
			return
		}
		select {
		case results <- opcode == 0x1b3:
		default:
		}
	})
	defer cancel()
	packet := make([]byte, 12)
	binary.LittleEndian.PutUint64(packet, trainer)
	binary.LittleEndian.PutUint32(packet[8:], spell)
	if err := human.World.SendPacketRaw(0x1b2, packet); err != nil {
		t.Fatal(err)
	}
	select {
	case received := <-results:
		if received != success {
			t.Fatalf("Trainer purchase %d: success=%v, expected=%v", spell, received, success)
		}
	case <-time.After(10 * time.Second):
		t.Fatalf("Trainer sent no result for spell %d", spell)
	}
	if success {
		awaitState(t, "the purchased spell in the actual spell book", 5*time.Second,
			func() bool { return human.World.KnowsSpell(spell) })
	} else if human.World.KnowsSpell(spell) {
		t.Fatalf("Rejected purchase taught spell %d", spell)
	}
}

func TestBackportRaceAndNecromancerCompanion(t *testing.T) {
	chars := openDatabase(t, "E2E_CHAR_DSN")
	human := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "AdvNec", Race: 13, Class: 9, Level: 50})
	for _, spell := range []uint32{669, 363402, 363404, 363405, 363406} {
		if !human.World.KnowsSpell(spell) {
			t.Fatalf("New male Horde Dracthyr lacks required racial spell %d", spell)
		}
	}
	if human.World.KnowsSpell(363403) {
		t.Fatal("Male Dracthyr learned the female Visage")
	}
	human.GM(t, ".gm off")
	human.CastMust(t, 363402, human.GUID, 5*time.Second)
	awaitState(t, "cosmetic Visage aura", 5*time.Second, func() bool { return human.HasAura(363402) })
	human.GoCreatureID(t, 95023)
	var trainer uint64
	awaitState(t, "Orgrimmar Necromancer instructor", 10*time.Second, func() bool {
		trainer = human.World.FindUnitByEntry(95023, 10)
		return trainer != 0
	})
	if human.World.Money() != 0 {
		t.Fatalf("Fresh fixture unexpectedly has %d copper", human.World.Money())
	}
	trainerPurchase(t, human, trainer, 363411, false)
	e2eharness.ModMoney(t, human.World, 100000)
	awaitState(t, "fixture training funds", 5*time.Second, func() bool { return human.World.Money() == 100000 })
	before := human.World.Money()
	trainerPurchase(t, human, trainer, 363411, true)
	awaitState(t, "native trainer payment", 5*time.Second, func() bool { return human.World.Money() < before })
	mana, _ := human.World.Power()
	human.CastMust(t, 363411, human.GUID, 15*time.Second)
	awaitState(t, "summon mana expenditure", 3*time.Second, func() bool {
		current, _ := human.World.Power()
		return current < mana
	})
	var pet uint64
	awaitState(t, "native active companion field", 10*time.Second, func() bool {
		pet = human.World.GetObject(human.GUID).GUIDField(0x0008)
		object := human.World.GetObject(pet)
		return pet != 0 && object != nil && object.Entry == 95000
	})
	human.GM(t, ".save")
	awaitState(t, "durable native undead companion", 10*time.Second, func() bool {
		var count int
		err := chars.QueryRow("SELECT COUNT(*) FROM character_pet WHERE owner=? AND entry=95000", human.GUID).Scan(&count)
		if err != nil {
			t.Fatal(err)
		}
		return count == 1
	})
	t.Logf("PASS: Horde Dracthyr racials, rejected unaffordable training, paid spell, real mana cost and retained pet %x", pet)
}

func TestBackportCultistTrainer(t *testing.T) {
	human := e2eharness.NewSolo(t, e2eharness.ScenarioOpts{Prefix: "AdvCul", Race: 12, Class: 5, Level: 40})
	human.GoCreatureID(t, 95020)
	var trainer uint64
	awaitState(t, "Stormwind Cultist instructor", 10*time.Second, func() bool {
		trainer = human.World.FindUnitByEntry(95020, 10)
		return trainer != 0
	})
	e2eharness.ModMoney(t, human.World, 100000)
	awaitState(t, "fixture training funds", 5*time.Second, func() bool { return human.World.Money() == 100000 })
	before := human.World.Money()
	trainerPurchase(t, human, trainer, 363411, false)
	if human.World.Money() != before {
		t.Fatal("Wrong-class spell request consumed money")
	}
	trainerPurchase(t, human, trainer, 363420, true)
	awaitState(t, "paid Cultist training", 5*time.Second, func() bool { return human.World.Money() < before })
	t.Log("PASS: Alliance Dracthyr creation and paid Cultist training; wrong-class purchase rejected")
}
