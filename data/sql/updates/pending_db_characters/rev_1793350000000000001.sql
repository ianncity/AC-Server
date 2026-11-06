-- Remove learned and persisted Dracthyr Visage effects from existing characters.
DELETE character_spell
FROM `character_spell` AS character_spell
INNER JOIN `characters` AS characters ON characters.guid = character_spell.guid
WHERE characters.race IN (12, 13) AND character_spell.spell IN (363402, 363403);

DELETE character_aura
FROM `character_aura` AS character_aura
INNER JOIN `characters` AS characters ON characters.guid = character_aura.guid
WHERE characters.race IN (12, 13) AND character_aura.spell IN (363402, 363403, 372014);
