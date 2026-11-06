# llama.cpp inference integration

ADVAI sends HTTP inference requests to an existing llama.cpp server at
`POST /v1/chat/completions`. It does not start llama.cpp, load/download a model,
or run inference inside worldserver. The server's already-loaded default model is used.
The server must support llama.cpp's `response_format.type=json_schema` with a `schema` object.
It must also expose `/tokenize`, `/detokenize` and `/apply-template`; these enforce
actual model-token limits before generation. Missing endpoints fail visibly.

Apply `conf/playerbots.advai.llm.preset` after the normal ADVAI preset, adjusting
the numeric loopback address and port to your server. This is opt-in; the ordinary
ADVAI preset keeps inference disabled. These source additions require a future native build.

Four background HTTP workers submit inference requests by default. Game threads never
wait for inference. Defaults allow 120 requests/minute globally, including at most
12 bot requests/minute. The combined waiting/result queue is bounded to 64 plus up
to ten in-flight requests. Set `Playerbots.AdvAI.LLM.Workers` to the server's
`--parallel` slot count (4-10), or lower when reserving capacity for other clients.
Only that many HTTP requests can run simultaneously; additional requests wait in
the module queue. Humans have priority, with FIFO ordering within each priority.
HTTP response bodies are limited to 64 KiB; generation to 160 tokens. The entire
user context, including personality, gameplay facts and recent conversation, is
hard-capped at 1024 actual model tokens. Compact, labeled fact lines replace the
verbose JSON snapshot. Current message, identity, personality, goals, relationship,
needs and the two most recent chat lines come first. Relevant linked/named items
and spells precede general inventory/spell samples. Up to eight items, eight spells,
three shared experiences and four older chat lines can follow, subject to the same
token cap. Candidates are a bounded view, not a complete searchable world database.

Human whispers and direct mentions can use inference. Selected bot conversations
use the same delivered-message tracking, participation limit, channel permissions,
duplicate suppression and six-message exchange budget. Explicit Playerbots commands
keep their existing parsing and permissions. Questions such as “Could you cast
Fortitude on me?” can request a favor; explicit `cast ...` remains a base command.

Structured model decisions can request:

- `chat`: a short English response conditioned on supplied knowledge and personality.
- `inventory`: a server-rendered sample of actual carried items, refreshed at delivery.
- `buy`/`sell`: a proposal through existing native negotiation, stock, price and trade checks.
- `cast`: a known positive heal/buff/dispel/resource spell on the speaker, subject to
  native casting checks, current availability and server-computed willingness.
- `give`: a spare consumable stack to a sufficiently liked group member, preserving
  at least ten units and existing reservations; uses the ordinary trade flow.
- `invite`: a normal party invitation subject to willingness and native eligibility.

Item/spell IDs outside the supplied candidates are rejected. Inference does not
authorize commands, execute scripts, award resources, or directly edit persistent
game state. Action replies come from native outcomes; a started cast is not reported
as a confirmed applied buff. Gifts and sales still require the recipient to complete
the normal trade. No price/quantity extraction beyond existing negotiation is added;
the initial model proposal uses native default pricing/stack selection, followed by
ordinary agreement. One model action is accepted per turn.

New messages invalidate older pending inference responses for that bot. Results also
expire after `Playerbots.AdvAI.LLM.QueueSeconds` (120 seconds by default, including
queue wait) and are rejected after logout, location/activity changes or channel
permission changes. Worker HTTP timeouts default to 15 seconds and begin after
dequeueing. Superseded queued requests are cancelled when a newer message arrives.
The queue is bounded deliberately: exhausted queue/rate budgets and expired work
are rejected rather than accumulating an unlimited backlog. Connection failures,
invalid model output, missing candidates and queue exhaustion produce operator logs;
failed inference does not silently select an authored conversation response.

`advai stats` includes `llm_slots`, `llm_inflight`, `llm_queued`, `llm_results`, `llm_completed`, `llm_errors` and
`llm_dropped`, plus `llm_context_max`, `llm_prompt_max` and `llm_context_trimmed`.
Token maxima are actual server-tokenizer counts; trimmed counts requests whose
context was shortened. Recent LLM transcript lines stay bounded in RAM; existing durable social
events and personalities continue to persist. Free-form chat remains model-generated:
schema validation constrains structure and action IDs, not the factual correctness of
every sentence. Native build, inference-server compatibility and in-game acceptance
have not been run for this implementation.

Protocol reference: https://github.com/ggml-org/llama.cpp/blob/HEAD/tools/server/README.md

## Slot context sizing

Target **2048 tokens per slot**. Compact user context aims for 512-1024 tokens
when useful facts are available; sparse contexts are kept shorter rather than padded.
The hard ceiling is 1024, measured through the loaded model's `/tokenize` endpoint.
If exceeded, only the first 1008 tokens are decoded, the incomplete trailing record
is removed, and the result is tokenized again to verify the ceiling. Names and facts
are never cut mid-record. Item/spell candidates removed from the text are also removed
from the server's permitted decision IDs.

`/apply-template` formats the exact system/user messages, then `/tokenize` counts
the full prompt including model-specific formatting. The request is admitted only
when **prompt tokens + 160 output tokens + 32 boundary tokens <= 2048**; that is,
the formatted prompt may use at most **1856 tokens**. An unusually large/incompatible
template fails visibly. The response grammar is sent through `response_format`;
the compact system prompt explicitly names the supported actions. Thinking is
disabled through `chat_template_kwargs.enable_thinking=false` for templates that
support it, keeping the short generation budget for the actual decision/reply.

Normal requests perform three metadata calls (context tokenization, template
formatting, prompt tokenization) before the completion. Truncation adds detokenization
and a verification tokenization. All calls run on the same bounded background worker,
share its HTTP deadline, and require no additional model generation. No tokenizer
or template requests run on game threads.

Persistent personality and relationships live in ADVAI; slots do not need a bot's
whole lifetime transcript. Slots can serve any bot, since every request supplies its context.

For servers allocating `--ctx-size` across slots, use `--parallel 4 --ctx-size 8192`
or `--parallel 10 --ctx-size 20480`. Current llama.cpp also documents
`--kv-unified-per-slot 2048`, which sizes the shared KV pool from the per-slot
limit when `--ctx-size` is omitted. Use the option supported by your installed
server and confirm its reported per-slot context. These are configuration examples;
ADVAI does not launch or reconfigure the inference server.

The system prompt keeps identity, conversational style, factual limits and action
semantics concise. The user message supplies only bounded facts and recent turns.
Replies stay short, use the persistent speech style, avoid stock greetings and
narration, and may ask a follow-up, disagree, joke or end naturally.

## Expanded requests and bot-to-bot interaction

The schema now accepts 23 decision types. In addition to the original requests:

| Request examples | Execution |
| --- | --- |
| “What can you cast?”, “What are you short of?”, “What are you working toward?” | Native spell samples and cached needs/goal facts. |
| “Which quest?”, “What professions?”, “Where are you?”, “Ready?” | Existing factual conversation handlers. |
| “Could you make [item link]?” | Resolve an actual known recipe and commit a crafting goal; existing crafting, materials and travel actions perform it. Completion is observed later. |
| “You should repair/restock/train first” | Willing bots accept a preparation goal when the corresponding need exists, preserving interrupted work and retry bounds. |
| “Can I join your guild?” | Native guild invitation and rank/membership checks. |
| “Focus that healer”, “Peel for me”, “Regroup” | Same-group PvP request handling and visible targets; regroup uses the registered native action. |
| “Call it off”, “Changed my mind”, “Maybe later” | Broader native transaction cancellation; agreement accepts existing conversational consent phrases. |

Relevant known recipe products join the bounded item candidates, allowing named craft
requests where a matching known recipe is found. An item link remains the most precise
way to identify a product. Crafting acceptance does not promise free ingredients or
delivery to the requester. Preparation requests respect existing activity commitments.

During an ambient opportunity, a bot can choose an actual visible nearby playerbot
as a conversation partner. Up to 24 cached nearby candidates are considered. Familiarity,
shared positive quest/help experiences, guild membership, group membership and similar
levels influence selection. Disliked contacts and unavailable peers are excluded. A
ten-minute cooldown discourages repeatedly approaching the same peer.

The initiating inference is explicitly marked as an initiative with no incoming message.
It can produce only chat; it cannot assume the peer's consent or perform a gameplay
action. It may ask about supplies, invite company, discuss a goal or refer to recorded
help/setbacks. Only after the actual utterance is delivered can the other bot answer
or choose an action. Normal willingness, inventory, recipe, spell, group and guild
checks apply to bot speakers too. Existing native group acceptance and reciprocal
gift/trade mechanics execute cooperation when both participants agree and are eligible.
Small talk can develop into invitations, supply exchanges, crafting goals or buff requests;
declines and unavailable resources can also end it.

These interactions retain global inference limits, the smaller bot inference budget,
five-participant/six-message limits and delivered-message tracking. Structured action
responses retain the original conversation thread. Added context includes preferred
activity, current quest, profession skill, supply/preparation needs and observable peer
group/guild membership. Opinions and questions draw on those facts and recent memory.
Ordinary authored chat also gains 24 lines, bringing English content to 3,324 variants;
apply `2026_10_17_07_advai_social_conversation.sql` with the other module updates.

This pass concerns playerbot-to-playerbot interactions. Build, actual inference-server
compatibility, conversational quality and observed cooperative behavior remain unverified.

## Plain English conversation pass

The English catalog gains another 96 authored lines across greetings, small talk,
clarification, help, grouping, disagreement, apologies, familiar contacts, jokes,
farewells and acknowledgments: 3420 variants across 55 existing outcomes. Apply
`2026_10_17_08_advai_plain_english.sql` with the other module updates.

Rule parsing recognizes more everyday phrasing, including "got room for me?",
"can I tag along?", "where can I find you?", "what are you working on?", "would you
mind", "any chance", common abbreviations and explicit requests to stop chatting.
Courtesy such as "no rush" and "no worries" no longer negates an otherwise positive
request; an actual additional refusal still does.

Inference instructions distinguish inventory/spell questions, crafting, gifts and
purchases. They require clarification for competing subjects and prohibit substituting
an unrelated item/spell for a named unavailable one. A bot remembers the last requested
item or spell for up to two minutes with the same partner, making "that one" and
"same again" useful follow-ups. This reference is only a subject, not a completion
record or consent. Topic changes, partner changes and expiry discard its use.
Referenced candidates still need to survive the 1024-token context cap and all native
action checks. General spell candidates collapse repeated names to the highest known
rank; an explicitly linked rank is preserved. No additional inference actions or
permission grants are introduced by these references.
