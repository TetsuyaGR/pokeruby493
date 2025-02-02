#include "global.h"
#include "constants/abilities.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"
#include "constants/items.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "battle.h"
#include "berry.h"
#include "data2.h"
#include "event_data.h"
#include "item.h"
#include "pokemon.h"
#include "ewram.h"

extern u16 gBattleTypeFlags;
extern struct BattlePokemon gBattleMons[4];
extern u16 gCurrentMove;
extern u8 gCritMultiplier;
extern u16 gBattleWeather;
extern struct BattleEnigmaBerry gEnigmaBerries[];
extern u16 gBattleMovePower;
extern u16 gTrainerBattleOpponent;

// Masks for getting PP Up count, also PP Max values
const u8 gPPUpReadMasks[] = {0x03, 0x0c, 0x30, 0xc0};

// Masks for setting PP Up count
const u8 gPPUpWriteMasks[] = {0xFC, 0xF3, 0xCF, 0x3F};

// Values added to PP Up count
const u8 gPPUpValues[] = {0x01, 0x04, 0x10, 0x40};

const u8 gStatStageRatios[][2] =
{
    {10, 40}, // -6
    {10, 35}, // -5
    {10, 30}, // -4
    {10, 25}, // -3
    {10, 20}, // -2
    {10, 15}, // -1
    {10, 10}, //  0
    {15, 10}, //  1
    {20, 10}, //  2
    {25, 10}, //  3
    {30, 10}, //  4
    {35, 10}, //  5
    {40, 10}  //  6
};

const u8 unknownGameFreakAbbrev_820825E[] = _("ゲーフリ");

u8 GetBattlerSide(u8 bank);

#define APPLY_STAT_MOD(var, mon, stat, statIndex)                                   \
{                                                                                   \
    (var) = (stat) * (gStatStageRatios)[(mon)->statStages[(statIndex)]][0];         \
    (var) /= (gStatStageRatios)[(mon)->statStages[(statIndex)]][1];                 \
}

#define BADGE_BOOST(badge, stat, bank) ({ \
if (!(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_BATTLE_TOWER | BATTLE_TYPE_EREADER_TRAINER))) \
{ \
    if ((gBattleTypeFlags & BATTLE_TYPE_TRAINER) \
    && gTrainerBattleOpponent != SECRET_BASE_OPPONENT \
    && FlagGet(FLAG_BADGE0##badge##_GET) \
    && GetBattlerSide(bank) == B_SIDE_PLAYER) \
        (stat) = (110 * (stat)) / 100; \
} \
})

s32 CalculateBaseDamage(struct BattlePokemon *attacker, struct BattlePokemon *defender, u32 move, u16 sideStatus, u16 powerOverride, u8 typeOverride, u8 bankAtk, u8 bankDef)
{
    s32 damage = 0;
    s32 damageHelper;
    u8 type;
    u16 attack, defense;
    u16 spAttack, spDefense;
    u8 defenderHoldEffect;
    u8 defenderHoldEffectParam;
    u8 attackerHoldEffect;
    u8 attackerHoldEffectParam;

    if (!powerOverride)
        gBattleMovePower = gBattleMoves[move].power;
    else
        gBattleMovePower = powerOverride;

    if (!typeOverride)
        type = gBattleMoves[move].type;
    else
        type = typeOverride & 0x3F;

    attack = attacker->attack;
    defense = defender->defense;
    spAttack = attacker->spAttack;
    spDefense = defender->spDefense;

    if (attacker->item == ITEM_ENIGMA_BERRY)
    {
        attackerHoldEffect = gEnigmaBerries[bankAtk].holdEffect;
        attackerHoldEffectParam = gEnigmaBerries[bankAtk].holdEffectParam;
    }
    else
    {
        attackerHoldEffect = ItemId_GetHoldEffect(attacker->item);
        attackerHoldEffectParam = ItemId_GetHoldEffectParam(attacker->item);
    }

    if (defender->item == ITEM_ENIGMA_BERRY)
    {
        defenderHoldEffect = gEnigmaBerries[bankDef].holdEffect;
        defenderHoldEffectParam = gEnigmaBerries[bankDef].holdEffectParam;
    }
    else
    {
        defenderHoldEffect = ItemId_GetHoldEffect(defender->item);
        defenderHoldEffectParam = ItemId_GetHoldEffectParam(defender->item);
    }

    if (attacker->ability == ABILITY_HUGE_POWER || attacker->ability == ABILITY_PURE_POWER)
        attack *= 2;

    BADGE_BOOST(1, attack, bankAtk);
    BADGE_BOOST(5, defense, bankDef);
    BADGE_BOOST(7, spAttack, bankAtk);
    BADGE_BOOST(7, spDefense, bankDef);

    if ((attackerHoldEffect == HOLD_EFFECT_TYPE_BOOST || attackerHoldEffect == HOLD_EFFECT_PLATE)
        && type == attackerHoldEffectParam)
    {
        if (TYPE_IS_PHYSICAL(type))
            attack = (attack + (attack / 5));
        else
			spAttack = (spAttack + (spAttack / 5));
    }

    if (attackerHoldEffect == HOLD_EFFECT_MUSCLE_BAND)
        attack = (110 * attack) / 100;
    if (attackerHoldEffect == HOLD_EFFECT_WISE_GLASSES && (gBattleMoves[gCurrentMove].flags & F_MOVE_IS_SPECIAL))
        spAttack = (110 * spAttack) / 100;
    if (attackerHoldEffect == HOLD_EFFECT_CHOICE_BAND)
        attack = (150 * attack) / 100;
    if (attackerHoldEffect == HOLD_EFFECT_SOUL_DEW && !(gBattleTypeFlags & BATTLE_TYPE_BATTLE_TOWER) && (attacker->species == SPECIES_LATIAS || attacker->species == SPECIES_LATIOS))
        spAttack = (150 * spAttack) / 100;
    if (defenderHoldEffect == HOLD_EFFECT_SOUL_DEW && !(gBattleTypeFlags & BATTLE_TYPE_BATTLE_TOWER) && (defender->species == SPECIES_LATIAS || defender->species == SPECIES_LATIOS))
        spDefense = (150 * spDefense) / 100;
    if (attackerHoldEffect == HOLD_EFFECT_DEEP_SEA_TOOTH && attacker->species == SPECIES_CLAMPERL)
        spAttack *= 2;
    if (defenderHoldEffect == HOLD_EFFECT_DEEP_SEA_SCALE && defender->species == SPECIES_CLAMPERL)
        spDefense *= 2;
    if (attackerHoldEffect == HOLD_EFFECT_LIGHT_BALL && attacker->species == SPECIES_PIKACHU)
        spAttack *= 2, attack *= 2;
    if (defenderHoldEffect == HOLD_EFFECT_METAL_POWDER && defender->species == SPECIES_DITTO)
        defense *= 2;
    if (attackerHoldEffect == HOLD_EFFECT_THICK_CLUB && (attacker->species == SPECIES_CUBONE || attacker->species == SPECIES_MAROWAK))
        attack *= 2;
    if (defender->ability == ABILITY_THICK_FAT && (type == TYPE_FIRE || type == TYPE_ICE))
        spAttack /= 2;
	if (attacker->ability == ABILITY_SOLAR_POWER && (gBattleWeather & WEATHER_SUN_ANY))
		spAttack = (150 * spAttack) / 100;
    if (attacker->ability == ABILITY_HUSTLE)
        attack = (150 * attack) / 100;
    if (defender->ability == ABILITY_FLOWER_GIFT && (gBattleWeather & WEATHER_SUN_ANY))
        spDefense = (150 * spDefense) / 100;
    if (attacker->ability == ABILITY_FLOWER_GIFT && (gBattleWeather & WEATHER_SUN_ANY))
        attack = (150 * attack) / 100;
	if ((gBattleWeather & WEATHER_SUN_ANY) && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_FLOWER_GIFT, 0, 0))
        spDefense = (150 * spDefense) / 100;
    if ((gBattleWeather & WEATHER_SUN_ANY) && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_FLOWER_GIFT, 0, 0))
        attack = (150 * attack) / 100;
    if (attacker->ability == ABILITY_PLUS && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_MINUS, 0, 0))
        spAttack = (150 * spAttack) / 100;
    if (attacker->ability == ABILITY_MINUS && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_PLUS, 0, 0))
        spAttack = (150 * spAttack) / 100;
    if (attacker->ability == ABILITY_GUTS && attacker->status1)
        attack = (150 * attack) / 100;
    if (defender->ability == ABILITY_MARVEL_SCALE && defender->status1)
        defense = (150 * defense) / 100;
    if (attacker->species == SPECIES_DIALGA && attacker->item == ITEM_ADAMANT_ORB && (type == TYPE_STEEL || type == TYPE_DRAGON))
        gBattleMovePower = (120 * gBattleMovePower) / 100;
    if (attacker->species == SPECIES_PALKIA && attacker->item == ITEM_LUSTROUS_ORB && (type == TYPE_WATER || type == TYPE_DRAGON))
        gBattleMovePower = (120 * gBattleMovePower) / 100;
    if (attacker->species == SPECIES_GIRATINA && attacker->item == ITEM_GRISEOUS_ORB && (type == TYPE_GHOST || type == TYPE_DRAGON))
        gBattleMovePower = (120 * gBattleMovePower) / 100;
    if (type == TYPE_ELECTRIC && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, 0, 0xFD, 0))
        gBattleMovePower /= 2;
    if (type == TYPE_FIRE && AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, 0, 0xFE, 0))
        gBattleMovePower /= 2;
    if (type == TYPE_GRASS && attacker->ability == ABILITY_OVERGROW && attacker->hp <= (attacker->maxHP / 3))
        gBattleMovePower = (150 * gBattleMovePower) / 100;
    if (type == TYPE_FIRE && attacker->ability == ABILITY_BLAZE && attacker->hp <= (attacker->maxHP / 3))
        gBattleMovePower = (150 * gBattleMovePower) / 100;
    if (type == TYPE_WATER && attacker->ability == ABILITY_TORRENT && attacker->hp <= (attacker->maxHP / 3))
        gBattleMovePower = (150 * gBattleMovePower) / 100;
    if (type == TYPE_BUG && attacker->ability == ABILITY_SWARM && attacker->hp <= (attacker->maxHP / 3))
        gBattleMovePower = (150 * gBattleMovePower) / 100;
    if (gBattleMoves[gCurrentMove].effect == EFFECT_EXPLOSION)
        defense /= 2;
    if (attacker->ability == ABILITY_IRON_FIST && (gCurrentMove == MOVE_BULLET_PUNCH || gCurrentMove == MOVE_COMET_PUNCH || gCurrentMove == MOVE_DIZZY_PUNCH || gCurrentMove == MOVE_DRAIN_PUNCH || gCurrentMove == MOVE_DYNAMIC_PUNCH || gCurrentMove == MOVE_FIRE_PUNCH || gCurrentMove == MOVE_FOCUS_PUNCH|| gCurrentMove == MOVE_HAMMER_ARM || gCurrentMove == MOVE_ICE_PUNCH || gCurrentMove == MOVE_MACH_PUNCH || gCurrentMove == MOVE_MEGA_PUNCH || gCurrentMove == MOVE_METEOR_MASH || gCurrentMove == MOVE_SHADOW_PUNCH || gCurrentMove == MOVE_SKY_UPPERCUT || gCurrentMove == MOVE_THUNDER_PUNCH))
        gBattleMovePower = (120 * gBattleMovePower) / 100;
    if ((gBattleMoves[gCurrentMove].effect == EFFECT_DOUBLE_EDGE || gBattleMoves[gCurrentMove].effect == EFFECT_RECOIL_IF_MISS || gBattleMoves[gCurrentMove].effect == EFFECT_RECOIL) && attacker->ability == ABILITY_RECKLESS)
        gBattleMovePower = (120 * gBattleMovePower) / 100;
    if (gBattleMoves[gCurrentMove].power <= 60 && attacker->ability == ABILITY_TECHNICIAN)
        gBattleMovePower = (150 * gBattleMovePower) / 100;
    if (defender->ability == ABILITY_DRY_SKIN && (gBattleMoves[gCurrentMove].type == TYPE_FIRE))
        gBattleMovePower = (125 * gBattleMovePower) / 100;
	if (attacker->ability == ABILITY_SLOW_START && gDisableStructs[bankAtk].slowStartTimer <= 4)
        attack /= 2;

	// Rivalry
	if (GetGenderFromSpeciesAndPersonality(attacker->species, attacker->personality) != MON_GENDERLESS
		&& GetGenderFromSpeciesAndPersonality(defender->species, defender->personality) != MON_GENDERLESS
		&& attacker->ability == ABILITY_RIVALRY)
	{
		if (GetGenderFromSpeciesAndPersonality(attacker->species, attacker->personality)
		== GetGenderFromSpeciesAndPersonality(defender->species, defender->personality))
			gBattleMovePower = (125 * gBattleMovePower) / 100;
		else
			gBattleMovePower = (150 * gBattleMovePower) / 200;
	}

    if ((gBattleMoves[gCurrentMove].flags & F_MOVE_IS_SPECIAL) == 0)
    //if (TYPE_IS_PHYSICAL(type)) // type < TYPE_MYSTERY
    {
        if (defender->ability == ABILITY_UNAWARE)
            damage = attack;
        else if (gCritMultiplier == 2)
        {
            if (attacker->statStages[STAT_STAGE_ATK] > 6)
                APPLY_STAT_MOD(damage, attacker, attack, STAT_STAGE_ATK)
            else
                damage = attack;
        }
        else
            APPLY_STAT_MOD(damage, attacker, attack, STAT_STAGE_ATK)

        damage = damage * gBattleMovePower;
        damage *= (2 * attacker->level / 5 + 2);

        if (attacker->ability == ABILITY_UNAWARE)
            damageHelper = defense;
        else if (gCritMultiplier == 2)
        {
            if (defender->statStages[STAT_STAGE_DEF] < 6)
                APPLY_STAT_MOD(damageHelper, defender, defense, STAT_STAGE_DEF)
            else
                damageHelper = defense;
        }
        else
            APPLY_STAT_MOD(damageHelper, defender, defense, STAT_STAGE_DEF)

        damage = damage / damageHelper;
        damage /= 50;

        if ((attacker->status1 & STATUS_BURN) && attacker->ability != ABILITY_GUTS)
            damage /= 2;

        if ((sideStatus & SIDE_STATUS_REFLECT) && gCritMultiplier == 1)
        {
            if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) && CountAliveMons(2) == 2)
                damage = 2 * (damage / 3);
            else
                damage /= 2;
        }

        if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) && gBattleMoves[move].target == 8 && CountAliveMons(2) == 2)
            damage /= 2;

        // moves always do at least 1 damage.
        if (damage == 0)
            damage = 1;
    }

    if (type == TYPE_MYSTERY)
        damage = 0; // is ??? type. does 0 damage.

    if ((gBattleMoves[gCurrentMove].flags & F_MOVE_IS_SPECIAL) != 0)
    //if (TYPE_IS_SPECIAL(type)) // type > TYPE_MYSTERY
    {
        if (defender->ability == ABILITY_UNAWARE)
            damage = spAttack;
        else if (gCritMultiplier == 2)
        {
            if (attacker->statStages[STAT_STAGE_SPATK] > 6)
                APPLY_STAT_MOD(damage, attacker, spAttack, STAT_STAGE_SPATK)
            else
                damage = spAttack;
        }
        else
            APPLY_STAT_MOD(damage, attacker, spAttack, STAT_STAGE_SPATK)

        damage = damage * gBattleMovePower;
        damage *= (2 * attacker->level / 5 + 2);

        if (attacker->ability == ABILITY_UNAWARE)
            damageHelper = spDefense;
        else if (gCritMultiplier == 2)
        {
            if (defender->statStages[STAT_STAGE_SPDEF] < 6)
                APPLY_STAT_MOD(damageHelper, defender, spDefense, STAT_STAGE_SPDEF)
            else
                damageHelper = spDefense;
        }
        else
            APPLY_STAT_MOD(damageHelper, defender, spDefense, STAT_STAGE_SPDEF)

        damage = (damage / damageHelper);
        damage /= 50;

        if ((sideStatus & SIDE_STATUS_LIGHTSCREEN) && gCritMultiplier == 1)
        {
            if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) && CountAliveMons(2) == 2)
                damage = 2 * (damage / 3);
            else
                damage /= 2;
        }

        if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE) && gBattleMoves[move].target == 8 && CountAliveMons(2) == 2)
            damage /= 2;

        // are effects of weather negated with cloud nine or air lock
        if (!AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_CLOUD_NINE, 0, 0)
            && !AbilityBattleEffects(ABILITYEFFECT_FIELD_SPORT, 0, ABILITY_AIR_LOCK, 0, 0))
        {
            if (gBattleWeather & WEATHER_RAIN_TEMPORARY)
            {
                switch (type)
                {
                case TYPE_FIRE:
                    damage /= 2;
                    break;
                case TYPE_WATER:
                    damage = (15 * damage) / 10;
                    break;
                }
            }

            // any weather except sun weakens solar beam
            if ((gBattleWeather & (WEATHER_RAIN_ANY | WEATHER_SANDSTORM_ANY | WEATHER_HAIL)) && gCurrentMove == MOVE_SOLAR_BEAM)
                damage /= 2;

            // sunny
            if (gBattleWeather & WEATHER_SUN_ANY)
            {
                switch (type)
                {
                case TYPE_FIRE:
                    damage = (15 * damage) / 10;
                    break;
                case TYPE_WATER:
                    damage /= 2;
                    break;
                }
            }
        }

        // flash fire triggered
        if ((eFlashFireArr.arr[bankAtk] & 1) && type == TYPE_FIRE)
            damage = (15 * damage) / 10;
    }

    return damage + 2;
}
