
void Spell::EffectAdvaiUnsupported(SpellEffIndex index)
{
    LOG_ERROR("spells", "ADVAI spell {} uses unimplemented effect {} in slot {}", m_spellInfo->Id,
              m_spellInfo->Effects[index].Effect, uint32(index));
    throw std::runtime_error("Unimplemented Ascension spell effect; the class content cannot be released");
}

void Spell::EffectAscensionTriggerSpellDelayed(SpellEffIndex index)
{
    if (effectHandleMode != SPELL_EFFECT_HANDLE_LAUNCH_TARGET && effectHandleMode != SPELL_EFFECT_HANDLE_LAUNCH)
        return;
    ScheduleAdvaiTrigger(index, uint32(std::max(1, damage)));
}

void Spell::EffectDelayedTriggerSpell(SpellEffIndex index)
{
    if (effectHandleMode != SPELL_EFFECT_HANDLE_LAUNCH_TARGET && effectHandleMode != SPELL_EFFECT_HANDLE_LAUNCH)
        return;
    ScheduleAdvaiTrigger(index, m_spellInfo->Effects[index].Amplitude);
}

void Spell::ScheduleAdvaiTrigger(SpellEffIndex index, uint32 delay)
{
    Unit* caster = m_caster->ToUnit();
    if (!caster || !delay)
        return;
    auto const& effect = m_spellInfo->Effects[index];
    auto const* trigger = sSpellMgr->GetSpellInfo(effect.TriggerSpell);
    if (!trigger)
        throw std::runtime_error("Missing delayed Ascension spell " + std::to_string(effect.TriggerSpell));
    bool needsCaster = trigger->NeedsToBeTriggeredByCaster(m_spellInfo, index);
    if (effectHandleMode == SPELL_EFFECT_HANDLE_LAUNCH_TARGET && !needsCaster)
        return;
    if (effectHandleMode == SPELL_EFFECT_HANDLE_LAUNCH && needsCaster &&
        (effect.GetProvidedTargetMask() & TARGET_FLAG_UNIT_MASK))
        return;
    Unit* target = unitTarget ? unitTarget : m_targets.GetUnitTarget();
    if (!target)
        target = caster;
    ObjectGuid casterGuid = caster->GetGUID();
    ObjectGuid targetGuid = target->GetGUID();
    ObjectGuid original = m_originalCasterGUID;
    uint32 mapId = caster->GetMapId();
    uint32 instance = caster->GetInstanceId();
    uint32 spellId = trigger->Id;
    bool destination = m_targets.HasDst();
    SpellDestination location;
    if (destination)
        location = *m_targets.GetDst();
    caster->m_Events.AddEventAtOffset(
        [casterGuid, targetGuid, original, mapId, instance, spellId, destination, location]()
        {
            Map* map = sMapMgr->FindMap(mapId, instance);
            if (!map)
                return;
            Unit* source = casterGuid.IsPlayer() ? ObjectAccessor::FindConnectedPlayer(casterGuid)
                          : casterGuid.IsPet() ? static_cast<Unit*>(map->GetPet(casterGuid))
                                              : static_cast<Unit*>(map->GetCreature(casterGuid));
            if (!source || !source->IsInWorld() || source->FindMap() != map)
                return;
            Unit* resolved = ObjectAccessor::GetUnit(*source, targetGuid);
            if (!resolved || !resolved->IsInWorld() || !source->InSamePhase(resolved))
                return;
            SpellCastTargets targets;
            targets.SetUnitTarget(resolved);
            if (destination)
                targets.SetDst(location);
            source->CastSpell(targets, sSpellMgr->GetSpellInfo(spellId), nullptr, TRIGGERED_FULL_MASK, nullptr, nullptr,
                              original);
        },
        Milliseconds(delay));
}
