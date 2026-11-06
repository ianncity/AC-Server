-- Remove trainer entries backed by deprecated, NYI, placeholder, or effectless source abilities.
DELETE FROM `trainer_spell`
WHERE (`TrainerId`, `SpellId`) IN (
    (98000, 801965),
    (98000, 806107),
    (98001, 806151),
    (98001, 500979),
    (98001, 802124)
);
