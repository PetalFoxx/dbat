#include "dbat/attack.h"
#include "dbat/combat.h"
#include "dbat/utils.h"
#include "dbat/comm.h"
#include "dbat/techniques.h"
#include "dbat/class.h"
#include "dbat/races.h"
#include "dbat/fight.h"
#include "dbat/guild.h"

namespace atk {
    std::map<int, std::vector<std::pair<int, int>>> minimumSkillRequired;

    std::map<int, std::vector<std::pair<int, int>>> trainSkillOnSuccess;

    std::map<int, std::vector<std::pair<int, int>>> trainSkillOnFailure;

    std::vector<int> comboSkills = {
            SKILL_PUNCH, SKILL_KICK, SKILL_ELBOW, SKILL_KNEE, SKILL_ROUNDHOUSE, SKILL_UPPERCUT,
            SKILL_SLAM, SKILL_HEELDROP, SKILL_BASH, SKILL_HEADBUTT, SKILL_TAILWHIP
    };

    Attack::Attack(struct char_data *ch, char *arg) : user(ch) {
        if(arg && strlen(arg)) input = arg;
        boost::trim(input);
        boost::split(args, input, boost::is_space());
    }

    std::optional<int> Attack::limbsToCheck() {
        return std::nullopt;
    }

    std::optional<int> Attack::hurtInjuredLimbs() {
        return std::nullopt;
    }

    bool Attack::checkLimbs() {
        auto toCheck = limbsToCheck();
        if(!toCheck) {
            return true;
        }
        return limb_ok(user, *toCheck);
    }

    bool Attack::checkSkills() {
        auto needed = minimumSkillRequired.find(getSkillID());
        if(needed == minimumSkillRequired.end()) return true;

        for(auto &s : needed->second) {
            auto skillID = s.first;
            auto required = s.second;
            if(GET_SKILL(user, skillID) < required) {
                send_to_char(user, "You need %d in %s to use this attack.\r\n", required, spell_info[skillID].name);
                return false;
            }
        }
        return true;
    }

    void Attack::doTrain(const std::vector<std::pair<int, int>> &skills) {
        for(auto &s : skills) {
            improve_skill(user, s.first, s.second);
        }
    }


    bool Attack::checkEmptyHands() {
        if(needEmptyHands()) {
            if (GET_EQ(user, WEAR_WIELD1) && GET_EQ(user, WEAR_WIELD2)) {
                send_to_char(user, "Your hands are full!\r\n");
                return false;
            }
        }
        return true;
    }

    bool Attack::checkOtherConditions() {
        return true;
    }

    int64_t Attack::calculateHealthCost() {
        return 0;
    }

    int64_t Attack::calculateStaminaCost() {
        return 0;
    }

    double Attack::calculateKiCost() {
        return 0.0;
    }

    bool Attack::checkCosts() {
        currentHealthCost = calculateHealthCost();
        if(GET_HIT(user) < currentHealthCost) {
            send_to_char(user, "You don't have enough health to do that!\r\n");
            return false;
        }
        currentStaminaCost = calculateStaminaCost();
        if(user->getCurST() < currentStaminaCost) {
            send_to_char(user, "You don't have enough stamina to do that!\r\n");
            return false;
        }
        currentKiCost = calculateKiCost();
        if(GET_CHARGE(user) < (currentKiCost * GET_MAX_MANA(user))) {
            send_to_char(user, "You don't have enough ki to do that!\r\n");
            return false;
        }
        return true;
    }

    std::optional<int> Attack::hasCooldown() {
        return std::nullopt;
    }

    bool Attack::getOpponent() {
        if (args.empty() && !FIGHTING(user)) {
            send_to_char(user, "Direct it at who?\r\n");
            return false;
        }

        if(args.empty()) {
            victim = FIGHTING(user);
        } else {
            if(!tech_handle_targeting(user, (char*)args[0].c_str(), &victim, &obj)) return;
        }
        return true;
    }

    void Attack::execute() {

        if(!can_grav(user)) return;
        if(!checkSkills()) return;
        if(!checkLimbs()) return;
        if(!checkEmptyHands()) return;
        if(!checkOtherConditions()) return;

        if(!getOpponent()) return;

        if(!checkCosts()) return;

        initSkill = init_skill(user, getSkillID());

        processAttack();
    }

    void Attack::processAttack() {
        switch(doAttack()) {
            case Result::Landed:
                onLanded();
                onLandedOrMissed();
                break;
            case Result::Missed:
                onMissed();
                onLandedOrMissed();
                break;
            case Result::Canceled:
                onCanceled();
                break;
        }
    }

    Result Attack::doAttack() {

        if(victim) return attackCharacter();
        else if(obj) return attackObject();
        else {
            send_to_char(user, "You can't find that target.\r\n");
            return Result::Canceled;
        }

    }

    int Attack::canKillType() {
        return 0;
    }

    DefenseResult Attack::attackOutcome(char_data* user, char_data* victim, int skillID, bool kiAttack) {
        initStats();

        currentHitProbability = roll_accuracy(user, init_skill(user, skillID), kiAttack);
        currentChanceToHit = chance_to_hit(user) * (1 + user->getAffectModifier(APPLY_ACCURACY));

        if(isPhysical() && !usesWeapon()) {
            if (IS_KABITO(user) && !IS_NPC(user)) {
                if (GET_SKILL_BASE(user, SKILL_STYLE) >= 75)
                    currentChanceToHit -= currentChanceToHit * 0.2;
            }
        }

        currentSpeedIndexCheck = handle_speed(user, victim);

        currentHitProbability += currentSpeedIndexCheck;

        handle_defense(victim, &currentParryCheck, &currentBlockCheck, &currentDodgeCheck);

        tech_handle_posmodifier(victim, currentParryCheck, currentBlockCheck, currentDodgeCheck, currentHitProbability);

        if(isKiAttack() && !isPhysical()) {
            if (IS_ICER(user) && rand_number(1, 30) >= 28) {
                actUser("@C$N@c disappears, avoiding the attack before reappearing elsewhere!@n");
                actVictim("@cYou disappear, avoiding the attack before reappearing elsewhere!@n");
                actRoom("@C$N@c disappears, avoiding the attack before reappearing elsewhere!@n");
                dodge_ki(user, victim, getHoming(), getAtkID(), initSkill, skillID); /* Effects on the room from dodging a ki attack
                            Num 1: [ 0 for non-homing, 1 for homing ki attacks, 2 for guided ]
                            Num 2: [ Number of attack for damtype ]*/
                pcost(victim, 0, GET_MAX_HIT(victim) / 150);
                pcost(user, currentKiCost, 0);
                return DefenseResult::Missed;
            }
        }

        handleAccuracyModifiers();


        if (GET_SKILL_PERF(user, getSkillID()) == 1) {
            attPerc += 0.05;
        }
        if (GET_SKILL_PERF(user, getSkillID()) == 2) {
            currentHitProbability += 5;
        }
        if (GET_SKILL_PERF(user, getSkillID()) == 3) {
            currentKiCost -= currentKiCost * 0.05;
        }

        calculateDamage();
        
        if(currentHitProbability < currentChanceToHit - 20) {
            // So you just missed...
            return DefenseResult::Missed;
        } else {
            // it was a clean hit! Or should be...

            if(victim->getAffectModifier(APPLY_PERFECT_DODGE) != 0) {
                return DefenseResult::Perfect_Dodged;
            }


            if(victim->getCurST() > 0) {
                return calculateDefense();
            }

            //Victim failed to defend, we have a clean hit!
            return DefenseResult::Failed;
        }
        //Default to fail
        return DefenseResult::Failed;
    }

    void Attack::handleAccuracyModifiers() {

    }

    void Attack::calculateDamage() {
        calcDamage = damtype(user, getAtkID(), initSkill, attPerc);
    }

    DefenseResult Attack::calculateDefense() {
        double dodgeChance = ((double) currentDodgeCheck / 5.0) * ((double) axion_dice(0) / 120.0) * (1.0 + victim->getAffectModifier(APPLY_DODGE_PERC));
        double blockChance = ((double) currentBlockCheck / 3.0) * ((double) axion_dice(0) / 120.0) * (1.0 + victim->getAffectModifier(APPLY_BLOCK_PERC));

        if(canZanzoken() && AFF_FLAGGED(user, AFF_ZANZOKEN) && canZanzoken())
            dodgeChance *= (2.0 * GET_SKILL(user, SKILL_ZANZOKEN)); 

        double overcomeDodge = (double) currentChanceToHit * ((double) axion_dice(0) / 120.0);
        double overcomeBlock = (double) currentChanceToHit * ((double) axion_dice(0) / 120.0);
        

        if(canParry() && calculateDeflect()) return DefenseResult::Parried;
        if(canDodge() && dodgeChance > overcomeDodge) return DefenseResult::Dodged;
        if(canBlock() && blockChance > overcomeBlock) return DefenseResult::Blocked;

        return DefenseResult::Failed;
    }

    Result Attack::attackCharacter() {
        if(!can_kill(user, victim, nullptr, canKillType())) return Result::Canceled;
        if (handle_defender(victim, user)) {
            victim = GET_DEFENDER(victim);
            // should I do can_kill again here???
        }

        DefenseResult result = attackOutcome(user, victim, getSkillID(), isKiAttack());
        if(result == DefenseResult::Blocked) return handleBlock();
        if(result == DefenseResult::Parried) return handleParry();
        if(result == DefenseResult::Dodged) return handleDodge();
        if(result == DefenseResult::Perfect_Dodged) return handlePerfectDodge();
        if(result == DefenseResult::Failed) return handleCleanHit();
        if(result == DefenseResult::Missed) return handleMiss();

    }

    Result Attack::handleCleanHit() {
        hitspot = roll_hitloc(user, victim, initSkill);
        announceHitspot();
        handleHitspot();
        /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
        int damLoc = 0;
        switch(hitspot) {
            case 1:
                damLoc = 4;
                break;
            case 2:
                damLoc = 3;
                break;
            case 3:
                damLoc = 4;
                break;
            case 4:
                damLoc = 1;
                break;
            case 5:
                damLoc = 2;
                break;
        }
        if(damLoc) dam_eq_loc(victim, damLoc);
        return Result::Landed;
    }

    void Attack::handleHitspot() {

    }

    void Attack::announceHitspot() {

    }

    Result Attack::handleOtherHit() {
        // Anything less than a clean hit costs half stamina.
        currentStaminaCost /= 2;

        if(victim->getCurST() > 0) {
            if(canParry() && calculateDeflect()) return handleParry();
            if(canDodge() && currentDodgeCheck > axion_dice(10)) return handleDodge();
            if(canBlock() && currentBlockCheck > axion_dice(10)) return handleBlock();
            return handleMiss();
        } else {
            // victim is unable to respond to our fumbled attack...
            return handleMiss();
        }
    }

    Result Attack::handleMiss() {
        announceMiss();
        return Result::Missed;
    }

    bool Attack::calculateDeflect() {
        double parryChance = ((double) currentParryCheck / 6.0) * ((double) axion_dice(0) / 120.0) * (1.0 + victim->getAffectModifier(APPLY_PARRY_PERC));
        double overcomeParry = (double) currentChanceToHit * ((double) axion_dice(0) / 120.0);

        return ((!IS_NPC(victim) || !MOB_FLAGGED(victim, MOB_DUMMY)) && parryChance > overcomeParry);
    }

    Result Attack::attackObject() {
        if(!can_kill(user, nullptr, obj, canKillType())) return Result::Canceled;
        calcDamage = calculateObjectDamage();
        announceObject();
        return Result::Landed;
    }

    void Attack::attackPreprocess() {

    }

    void Attack::attackPostprocess() {

    }

    void Attack::onLanded() {
        if(victim) {
            attackPreprocess();
            switch(defenseResult) {
                case DefenseResult::Blocked:
                    calcDamage /= 4;
                    break;
            }
            if(calcDamage < 1) calcDamage = 1;

            int divine = GET_SKILL(user, (int16_t) SkillID::DivineHalo);
            int vicDivine = GET_SKILL(victim, (int16_t) SkillID::DivineHalo);
            if(isKiAttack() && divine > 0 && divine >= axion_dice(0)) {
                send_to_char(user, "You feel your Halo intensify, purging the impurity of your attack.\n");
                send_to_room(user->getRoom(), "%s's halo flares, leaving their attack shimmering as it moves.\n", user->name);
                calcDamage *= divine / 2;
            }
            if(isKiAttack() && vicDivine > 0 && vicDivine >= axion_dice(0)) {
                send_to_char(user, "You feel your Halo intensify, burning away at your opponents attack.\n");
                send_to_room(user->getRoom(), "%s's halo flares, burning away part of the blast coming for them.\n", user->name);
                calcDamage /= (vicDivine / 50);
            }

            user->onAttack(*this);
            victim->onAttacked(*this);

            hurt(targetLimb, limbhurtChance(), user, victim, obj, calcDamage, isKiAttack());
            if(victim) {
                if(isPhysical()) tech_handle_fireshield(user, victim, getBodyPart().c_str());
                if(canCombo()) handle_multihit(user, victim);
            }
            attackPostprocess();
        } else if (obj) {
            if(calcDamage < 1) calcDamage = 1;
            hurt(0, 0, user, nullptr, obj, calcDamage, isKiAttack());
        }
    }

    void Attack::onMissed() {
        calcDamage = 0;
    }

    void Attack::onCanceled() {
        currentStaminaCost = 0;
        currentKiCost = 0;
        calcDamage = 0;
    }

    static std::map<int, std::string> bodyParts = {
            {0, "right arm"},
            {1, "left arm"},
            {2, "right leg"},
            {3, "left leg"}
    };

    void Attack::hurtLimb(int limb) {
        auto name = bodyParts[limb];
        send_to_char(user, fmt::format("Using your broken {} has damaged it more!@n\r\n", name).c_str());
        GET_LIMBCOND(user, limb) -= rand_number(3, 5);
        if (GET_LIMBCOND(user, limb) < 0) {
            act(fmt::format("@RYour {} has fallen apart!@n", name).c_str(), true, user, nullptr, nullptr, TO_CHAR);
            act(fmt::format("@r$n@R's {} has fallen apart!@n", name).c_str(), true, user, nullptr, nullptr, TO_ROOM);
        }
    }

    void Attack::doUserCost() {
        pcost(user, currentKiCost, currentStaminaCost);
    }

    void Attack::onLandedOrMissed() {
        auto atrain = autoTrainSkillID();
        if(atrain != -1) {
            improve_skill(user, getSkillID(), atrain);
        }
        if(cooldownOverride != -1)  {
            WAIT_STATE(user, cooldownOverride);
        } else if(auto cool = hasCooldown(); cool) {
            handle_cooldown(user, *cool);
        }
        if(currentStaminaCost < 0) currentStaminaCost = 0;
        if(currentKiCost < 0) currentKiCost = 0;
        if(currentStaminaCost > 0 || currentKiCost > 0) {
            doUserCost();
        }
        
        if(auto hlimb = hurtInjuredLimbs(); hlimb) {

            auto checkLimb = [&](int l) {return GET_LIMBCOND(user, l) > 0 && GET_LIMBCOND(user, l) < 50;};

            auto tohurt = *hlimb;
            // type 0 is arms, type 1 is legs. 2 is both.
            if(tohurt == 0 || tohurt == 2) {
                std::vector<int> limbs;
                for(auto i : {0, 1}) if(checkLimb(i)) limbs.emplace_back(i);
                if(!limbs.empty()) {
                    auto limb = Random::get(limbs);
                    hurtLimb(*limb);
                }
            }
            
            if(tohurt == 1 || tohurt == 2) {
                std::vector<int> limbs;
                for(auto i : {2, 3}) if(checkLimb(i)) limbs.emplace_back(i);
                if(!limbs.empty()) {
                    auto limb = Random::get(limbs);
                    hurtLimb(*limb);
                }
            }
        }
        postProcess();
        
    }

    void Attack::postProcess() {

    }

    void Attack::actUser(const std::string& msg, bool hideInvisible) {
        act(msg.c_str(), hideInvisible, user, obj, victim, TO_CHAR);
    }

    void Attack::actOthers(const std::string &msg, bool hideInvisible) {
        act(msg.c_str(), hideInvisible, user, obj, victim, TO_NOTVICT);
    }

    void Attack::actRoom(const std::string &msg, bool hideInvisible) {
        act(msg.c_str(), hideInvisible, user, obj, victim, TO_ROOM);
    }

    void Attack::actVictim(const std::string &msg, bool hideInvisible) {
        act(msg.c_str(), hideInvisible, user, obj, victim, TO_VICT);
    }

    bool Attack::canBlock() {
        return false;
    }

    bool Attack::canParry() {
        return false;
    }

    bool Attack::canDodge() {
        return false;
    }

    bool Attack::canZanzoken() {
        return true;
    }

    bool Attack::canCombo() {
        return false;
    }

    bool Attack::isPhysical() {
        return false;
    }

    bool Attack::isKiAttack() {
        return false;
    }

    bool Attack::usesWeapon() {
        return false;
    }

    int64_t Attack::calculateObjectDamage() {
        return damtype(user, getAtkID(), initSkill, attPerc);
    }

    void Attack::initStats() {
        currentParryCheck = 2;
        currentDodgeCheck = 2;
        currentBlockCheck = 2;
    }

    // MeleeAttack
    int64_t MeleeAttack::calculateObjectDamage() {
        return ((GET_HIT(user) / 10000) + (GET_STR(user)));
    }

    Result MeleeAttack::handleParry() {
        announceParry();
        defenseResult = DefenseResult::Parried;
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        improve_skill(victim, SKILL_PARRY, 0);
        return Result::Missed;

    }

    Result MeleeAttack::handleDodge() {
        announceDodge();
        defenseResult = DefenseResult::Dodged;
        improve_skill(victim, SKILL_DODGE, 0);
        if(canZanzoken() && AFF_FLAGGED(user, AFF_ZANZOKEN))
            pcost(victim, GET_MAX_MANA(victim) * 0.05, 0);
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        return Result::Missed;

    }

    Result MeleeAttack::handlePerfectDodge() {
        actVictim("@C$n@W moves so slowly that you dodge their attack with ease.@n");
        actUser("@WYou move quickly and yet @C$N@W simply sidesteps you!@n");
        actOthers("@C$n@W moves quickly and yet @c$N@W dodges with ease!@n");

        double incomingDamage = calcDamage * (1 + victim->getAffectModifier(APPLY_PERFECT_DODGE));


        if(victim->getCurKI() > 0) {
            if (currentDodgeCheck > axion_dice(10))
                incomingDamage /= 10;
            if(incomingDamage > victim->getMaxKI() / 5 && !incomingDamage > victim->getMaxKI() * 5)
                incomingDamage = victim->getMaxKI() / 5;

            
            victim->decCurKI(incomingDamage);

            int instinct = GET_SKILL(user, (int16_t) SkillID::InstinctualCombat);
            if(instinct >= axion_dice(20)) {
                actVictim("@C$n@W, without even thinking about it you lash out towards $N, using their own momentum against them.@n");
                actUser("@C$n@W, without breaking for a moment, lashes out, catching you offguard.@n");
                actOthers("@C$n@W, without breaking for a moment, lashes out, using their momentum against them, and catches $N offguard.@n");

                hurt(targetLimb, limbhurtChance(), victim, user, obj, (calcDamage / 2) * (instinct / 100), isKiAttack());
            }
        } else {
            victim->decCurST(1.5 * incomingDamage);
            actVictim("@WContinuing to dodge without Ki takes a heavy toll.@n");
        }

        return Result::Missed;
    }

    Result MeleeAttack::handleBlock() {
        announceBlock();
        defenseResult = DefenseResult::Blocked;
        improve_skill(victim, SKILL_BLOCK, 0);
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        return Result::Landed;

    }

    int64_t MeleeAttack::calculateStaminaCost() {
        return physical_cost(user, getSkillID());
    }

    void MeleeAttack::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= calc_critical(user, 0);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    // RangedAttack
    int RangedAttack::canKillType() {
        return 1;
    }

    void MeleeAttack::announceParry() {
        actUser("@C$N@W parries your " + getName() + " with a punch of $S own!@n");
        actVictim("@WYou parry @C$n's@W " + getName() + " with a punch of your own!@n");
        actOthers("@C$N@W parries @c$n's@W " + getName() + " with a punch of $S own!@n");
    }

    void MeleeAttack::announceBlock() {
        actUser("@C$N@W moves quickly and blocks your " + getName() + "!@n");
        actVictim("@WYou move quickly and block @C$n's@W " + getName() + "!@n");
        actOthers("@C$N@W moves quickly and blocks @c$n's@W " + getName() + "!@n");
    }

    void MeleeAttack::announceDodge() {
        actUser("@C$N@W manages to dodge your " + getName() + "!@n");
        actVictim("@WYou dodge @C$n's@W " + getName() + "!@n");
        actOthers("@C$N@W manages to dodge @c$n's@W " + getName() + "!@n");
    }

    void MeleeAttack::announceMiss() {
        actUser("@WYou can't believe it but your " + getName() + " misses!@n");
        actVictim("@C$n@W throws a " + getName() + " at you but somehow misses!@n");
        actOthers("@c$n@W throws a " + getName() + " at @C$N@W but somehow misses!@n");
    }

    void MeleeAttack::announceObject() {
        actUser("@WYou " + getName() + " $p@W as hard as you can!@n");
        actRoom("@C$n@W " + getName() + "s $p@W!@n");
    }

    // PUNCH
    void Punch::announceParry() {
        actUser("@C$N@W parries your punch with a punch of $S own!@n");
        actVictim("@WYou parry @C$n's@W punch with a punch of your own!@n");
        actOthers("@C$N@W parries @c$n's@W punch with a punch of $S own!@n");
    }

    void Punch::announceBlock() {
        actUser("@C$N@W moves quickly and blocks your punch!@n");
        actVictim("@WYou move quickly and block @C$n's@W punch!@n");
        actOthers("@C$N@W moves quickly and blocks @c$n's@W punch!@n");
    }

    void Punch::announceDodge() {
        actUser("@C$N@W manages to dodge your punch!@n");
        actVictim("@WYou dodge @C$n's@W punch!@n");
        actOthers("@C$N@W manages to dodge @c$n's@W punch!@n");
    }

    void Punch::announceMiss() {
        actUser("@WYou can't believe it but your punch misses!@n");
        actVictim("@C$n@W throws a punch at you but somehow misses!@n");
        actOthers("@c$n@W throws a punch at @C$N@W but somehow misses!@n");
    }

    void Punch::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou slam your fist into @C$N's@W body!@n");
                actVictim("@C$n@W slams $s fist into your body!@n");
                actOthers("@c$n@W slams $s fist into @C$N's@W body!@n");
                break;
            case 2:
                actUser("@WYou slam your fist into @C$N's@W face!@n");
                actVictim("@C$n@W slams $s fist into your face!@n");
                actOthers("@c$n@W slams $s fist into @C$N's@W face!@n");
                break;
            case 3:
                actUser("@WYou punch @C$N@W directly in the gut!@n");
                actVictim("@C$n@W punches you directly in the gut!@n");
                actOthers("@c$n@W punches @C$N@W directly in the gut!@n");
                break;
            case 4:
                actUser("@WYou punch @C$N@W in the arm!@n");
                actVictim("@C$n@W punches you in the arm!@n");
                actOthers("@c$n@W punches @C$N@W in the arm!@n");
                break;
            case 5:
                actUser("@WYou punch @C$N@W in the leg!@n");
                actVictim("@C$n@W punches you in the leg!@n");
                actOthers("@c$n@W punches @C$N@W in the leg!@n");
                break;
        }
    }

    void Punch::announceObject() {
        actUser("@WYou punch $p@W as hard as you can!@n");
        actRoom("@C$n@W punches $p@W extremely hard!@n");
    }

    // KICK
    void Kick::announceParry() {
        actUser("@C$N@W parries your kick with a kick of $S own!@n");
        actVictim("@WYou parry @C$n's@W kick with a kick of your own!@n");
        actOthers("@C$N@W parries @c$n's@W kick with a kick of $S own!@n");
    }

    void Kick::announceBlock() {
        actUser("@C$N@W moves quickly and blocks your kick!@n");
        actVictim("@WYou move quickly and block @C$n's@W kick!@n");
        actOthers("@C$N@W moves quickly and blocks @c$n's@W kick!@n");
    }

    void Kick::announceDodge() {
        actUser("@C$N@W manages to dodge your kick!@n");
        actVictim("@WYou dodge @C$n's@W kick!@n");
        actOthers("@C$N@W manages to dodge @c$n's@W kick!@n");
    }

    void Kick::announceMiss() {
        actUser("@WYou can't believe it, your kick misses!@n");
        actVictim("@C$n@W throws a kick at you, but thankfully misses!@n");
        actOthers("@c$n@W throws a kick at @C$N@W, but misses!@n");
    }

    void Kick::announceObject() {
        actUser("@WYou kick $p@W as hard as you can!@n");
        actRoom("@C$n@W kicks $p@W extremely hard!@n");
    }

    void Kick::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou slam your foot into @C$N's@W body!@n");
                actVictim("@C$n@W slams $s foot into your body!@n");
                actOthers("@c$n@W slams $s foot into @C$N's@W body!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou slam your foot into @C$N's@W face!@n");
                actVictim("@C$n@W slams $s foot into your face!@n");
                actOthers("@c$n@W slams $s foot into @C$N's@W face!@n");
                break;
            case 3:
                actUser("@WYou land your foot against @C$N's@W gut!@n");
                actVictim("@C$n@W lands $s foot against your gut!@n");
                actOthers("@C$n@W lands $s foot against @C$N's@W gut!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou land your foot against @C$N's@W leg!@n");
                actVictim("@C$n@W lands $s foot against your leg!@n");
                actOthers("@C$n@W lands $s foot against @C$N's@W leg!@n");
                break;
            case 5: /* Weak */
                actUser("@WYou land your foot against @C$N's@W arm!@n");
                actVictim("@C$n@W lands $s foot against your arm!@n");
                actOthers("@C$n@W lands $s foot against @C$N's@W arm!@n");
                break;
        }
    }

    // ELBOW
    void Elbow::announceParry() {
        actUser("@C$N@W parries your elbow with an elbow of $S own!@n");
        actVictim("@WYou parry @C$n's@W elbow with an elbow of your own!@n");
        actOthers("@C$N@W parries @c$n's@W elbow with an elbow of $S own!@n");
    }

    void Elbow::announceBlock() {
        actUser("@C$N@W blocks your elbow strike!@n");
        actVictim("@WYou block @C$n's@W elbow strike!@n");
        actOthers("@C$N@W blocks @c$n's@W elbow strike!@n");
    }

    void Elbow::announceDodge() {
        actUser("@C$N@W dodges your elbow strike!@n");
        actVictim("@WYou dodge @C$n's@W elbow strike!@n");
        actOthers("@C$N@W dodges @c$n's@W elbow strike!@n");
    }

    void Elbow::announceMiss() {
        actUser("@WYou can't believe it, your elbow misses!@n");
        actVictim("@C$n@W throws an elbow at you, but thankfully misses!@n");
        actOthers("@c$n@W throws an elbow at @C$N@W, but misses!@n");
    }

    void Elbow::announceObject() {
        actUser("@WYou elbow $p@W as hard as you can!@n");
        actRoom("@C$n@W elbows $p@W extremely hard!@n");
    }

    void Elbow::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou slam your elbow into @C$N's@W body!@n");
                actVictim("@C$n@W slams $s elbow into your body!@n");
                actOthers("@c$n@W slams $s elbow into @C$N's@W body!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou slam your elbow into @C$N's@W face!@n");
                actVictim("@C$n@W slams $s elbow into your face!@n");
                actOthers("@c$n@W slams $s elbow into @C$N's@W face!@n");
                break;
            case 3:
                actUser("@WYou land your elbow against @C$N's@W gut!@n");
                actVictim("@C$n@W lands $s elbow against your gut!@n");
                actOthers("@C$n@W lands $s elbow against @C$N's@W gut!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou land your elbow against @C$N's@W leg!@n");
                actVictim("@C$n@W lands $s elbow against your leg!@n");
                actOthers("@C$n@W lands $s elbow against @C$N's@W leg!@n");
                break;
            case 5: /* Weak */
                actUser("@WYou land your elbow against @C$N's@W arm!@n");
                actVictim("@C$n@W lands $s elbow against your arm!@n");
                actOthers("@C$n@W lands $s elbow against @C$N's@W arm!@n");
                break;
        }
    }


    // KNEE
    void Knee::announceParry() {
        actUser("@C$N@W parries your knee with a knee of $S own!@n");
        actVictim("@WYou parry @C$n's@W knee with a knee of your own!@n");
        actOthers("@C$N@W parries @c$n's@W knee with a knee of $S own!@n");
    }

    void Knee::announceBlock() {
        actUser("@C$N@W blocks your knee strike!@n");
        actVictim("@WYou block @C$n's@W knee strike!@n");
        actOthers("@C$N@W blocks @c$n's@W knee strike!@n");
    }

    void Knee::announceDodge() {
        actUser("@C$N@W dodges your knee strike!@n");
        actVictim("@WYou dodge @C$n's@W knee strike!@n");
        actOthers("@C$N@W dodges @c$n's@W knee strike!@n");
    }

    void Knee::announceMiss() {
        actUser("@WYou can't believe it, your knee strike misses!@n");
        actVictim("@C$n@W throws a knee strike at you, but thankfully misses!@n");
        actOthers("@c$n@W throws a knee strike at @C$N@W, but misses!@n");
    }

    void Knee::announceObject() {
        actUser("@WYou knee $p@W as hard as you can!@n");
        actRoom("@C$n@W knees $p@W extremely hard!@n");
    }

    void Knee::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou slam your knee into @C$N's@W body!@n");
                actVictim("@C$n@W slams $s knee into your body!@n");
                actOthers("@c$n@W slams $s knee into @C$N's@W body!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou slam your knee into @C$N's@W face!@n");
                actVictim("@C$n@W slams $s knee into your face!@n");
                actOthers("@c$n@W slams $s knee into @C$N's@W face!@n");
                break;
            case 3:
                actUser("@WYou land your knee against @C$N's@W gut!@n");
                actVictim("@C$n@W lands $s knee against your gut!@n");
                actOthers("@C$n@W lands $s knee against @C$N's@W gut!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou land your knee against @C$N's@W leg!@n");
                actVictim("@C$n@W lands $s knee against your leg!@n");
                actOthers("@C$n@W lands $s knee against @C$N's@W leg!@n");
                break;
            case 5: /* Weak */
                actUser("@WYou land your knee against @C$N's@W arm!@n");
                actVictim("@C$n@W lands $s knee against your arm!@n");
                actOthers("@C$n@W lands $s knee against @C$N's@W arm!@n");
                break;
        }
    }

    // ROUNDHOUSE
    std::optional<int> Roundhouse::hasCooldown() {
        if(IS_NAIL(user) && GET_SKILL_BASE(user, SKILL_STYLE) >= 75) return 5;
        return 7;
    }

    void Roundhouse::announceParry() {
        actUser("@C$N@W parries your roundhouse with a roundhouse of $S own!@n");
        actVictim("@WYou parry @C$n's@W roundhouse with a roundhouse of your own!@n");
        actOthers("@C$N@W parries @c$n's@W roundhouse with a roundhouse of $S own!@n");
    }

    void Roundhouse::announceBlock() {
        actUser("@C$N@W moves quickly and blocks your roundhouse!@n");
        actVictim("@WYou move quickly and block @C$n's@W roundhouse!@n");
        actOthers("@C$N@W moves quickly and blocks @c$n's@W roundhouse!@n");
    }

    void Roundhouse::announceDodge() {
        actUser("@C$N@W manages to dodge your roundhouse!@n");
        actVictim("@WYou dodge @C$n's@W roundhouse!@n");
        actOthers("@C$N@W manages to dodge @c$n's@W roundhouse!@n");
    }

    void Roundhouse::announceMiss() {
        actUser("@WYou can't believe it but your roundhouse misses!@n");
        actVictim("@C$n@W throws a roundhouse at you but somehow misses!@n");
        actOthers("@c$n@W throws a roundhouse at @C$N@W but somehow misses!@n");
    }

    void Roundhouse::announceObject() {
        actUser("@WYou roundhouse $p@W as hard as you can!@n");
        actRoom("@C$n@W roundhouses $p@W extremely hard!@n");
    }

    void Roundhouse::handleHitspot() {
        LegAttack::handleHitspot();
        if (hitspot == 2 && !AFF_FLAGGED(victim, AFF_FLYING) && GET_POS(victim) == POS_STANDING && rand_number(1, 8) >= 7) {
            handle_knockdown(victim);
        }
    }

    void Roundhouse::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou spin in mid air and land a kick into @C$N's@W body!@n");
                actVictim("@C$n@W spins in mid air and lands a kick into your body!@n");
                actOthers("@c$n@W spins in mid air and lands a kick into @C$N's@W body!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou spin a fierce roundhouse into @C$N's@W gut!@n");
                actVictim("@C$n@W spins a fierce roundhouse into your gut!@n");
                actOthers("@c$n@W spins a fierce roundhouse into @C$N's@W gut!@n");
                break;
            case 3:
                actUser("@WYou throw a roundhouse at @C$N@W, hitting $M directly in the neck!@n");
                actVictim("@C$n@W throws a roundhouse at you, hitting YOU directly in the neck!@n");
                actOthers("@c$n@W throws a roundhouse at @C$N@W, hitting $M directly in the face!@n");
                break;
            case 4: /* Weak */
                actUser("@WYour poorly aimed roundhouse hits @C$N@W in the arm!@n");
                actVictim("@C$n@W poorly aims a roundhouse and hits you in the arm!@n");
                actOthers("@c$n@W poorly aims a roundhouse and hits @C$N@W in the arm!@n");
                break;
            case 5: /* Weak 2 */
                actUser("@WYou slam a roundhouse into @C$N's@W leg!@n");
                actVictim("@C$n@W slams a roundhouse into your leg!@n");
                actOthers("@c$n@W slams a roundhouse into @C$N's@W leg!@n");
                break;
        }
    }

    int Roundhouse::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 195;
        }
        return 0;
    }


    // Uppercut
    std::optional<int> Uppercut::hasCooldown() {
        if (IS_FRIEZA(user) && GET_SKILL_BASE(user, SKILL_STYLE) >= 75) 
            handle_cooldown(user, 5);
        else
            handle_cooldown(user, 7);
    }

    void Uppercut::announceObject() {
        actUser("@WYou uppercut $p@W as hard as you can!@n");
        actRoom("@C$n@W uppercuts $p@W extremely hard!@n");
    }

    void Uppercut::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou leap up and launch an uppercut into @C$N's@W body!@n");
                actVictim("@C$n@W leaps up and launches an uppercut into your body!@n");
                actOthers("@c$n@W leaps up and launches an uppercut into @C$N's@W body!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou smash an uppercut into @C$N's@W chin!@n");
                actVictim("@C$n@W smashes an uppercut into your chin!@n");
                actOthers("@c$n@W smashes an uppercut into @C$N's@W chin!@n");
                break;
            case 3:
                actUser("@WYou uppercut @C$N@W, hitting $M directly in chest!@n");
                actVictim("@C$n@W uppercuts you, hitting you directly in the chest!@n");
                actOthers("@c$n@W uppercuts @C$N@W, hitting $M directly in the chest!@n");
                break;
            case 4: /* Weak */
                actUser("@WYour poorly aimed uppercut hits @C$N@W in the arm!@n");
                actVictim("@C$n@W poorly aims an uppercut and hits you in the arm!@n");
                actOthers("@c$n@W poorly aims an uppercut and hits @C$N@W in the arm!@n");
                break;
            case 5: /* Weak 2 */
                actUser("@WYou slam an uppercut into @C$N's@W leg!@n");
                actVictim("@C$n@W slams an uppercut into your leg!@n");
                actOthers("@c$n@W slams an uppercut into @C$N's@W leg!@n");
                break;
        }
    }

    void Uppercut::handleHitspot() {
        switch(hitspot) {
            case 1:
            case 2:
                if (GET_BONUS(user, BONUS_SOFT)) {
                    calcDamage *= calc_critical(user, 2);
                }
                if (!AFF_FLAGGED(victim, AFF_KNOCKED) &&
                        (rand_number(1, 8) >= 7 && (GET_HIT(victim) > GET_HIT(user) / 5) &&
                         !AFF_FLAGGED(victim, AFF_SANCTUARY))) {
                        act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@WYou are knocked out!@n", true, user, nullptr, victim, TO_VICT);
                        act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_NOTVICT);
                        victim->setStatusKnockedOut();
                }
                break;
            case 3:
                calcDamage *= calc_critical(user, 0);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }


    //Slam
    std::optional<int> Slam::hasCooldown() {
        if (IS_BARDOCK(user) && GET_SKILL_BASE(user, SKILL_STYLE) >= 75) 
            handle_cooldown(user, 7);
        else
            handle_cooldown(user, 9);
    }

    void Slam::announceObject() {
        actUser("@WYou slam $p@W as hard as you can!@n");
        actRoom("@C$n@W slams $p@W extremely hard!@n");
    }

    void Slam::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou disappear, appearing above @C$N@W and slam a double fisted blow into $M!@n");
                actVictim("@C$n@W disappears, only to appear above you, slamming a double fisted blow into you!@n");
                actOthers("@c$n@W disappears, only to appear above @C$N@W, slamming a double fisted blow into $M!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou disappear, reappearing in front of @C$N@W, you grab $M! Spinning you send $M flying into the ground!@n");
                actVictim("@C$n@W disappears, reappearing in front of you, and $e grabs you! Spinning quickly $e sends you flying into the ground!@n");
                actOthers("@c$n@W disappears, reappearing in front of @C$N@W, and grabs $M! Spinning quickly $e sends $M flying into the ground!@n");

                if (ROOM_DAMAGE(IN_ROOM(victim)) <= 95 && !ROOM_FLAGGED(IN_ROOM(victim), ROOM_SPACE)) {
                        act("@W$N@W slams into the ground forming a large crater with $S body!@n", true, user, nullptr,
                            victim,
                            TO_CHAR);
                        act("@WYou slam into the ground forming a large crater with your body!@n", true, user, nullptr,
                            victim,
                            TO_VICT);
                        act("@W$N@W slams into the ground forming a large crater with $S body!@n", true, user, nullptr,
                            victim,
                            TO_NOTVICT);
                        if (SECT(IN_ROOM(victim)) != SECT_INSIDE && SECT(IN_ROOM(victim)) != SECT_UNDERWATER &&
                            SECT(IN_ROOM(victim)) != SECT_WATER_SWIM && SECT(IN_ROOM(victim)) != SECT_WATER_NOSWIM) {
                            impact_sound(user, "@wA loud roar is heard nearby!@n\r\n");
                            switch (rand_number(1, 8)) {
                                case 1:
                                    act("Debris is thrown into the air and showers down thunderously!", true, user,
                                        nullptr,
                                        victim, TO_CHAR);
                                    act("Debris is thrown into the air and showers down thunderously!", true, user,
                                        nullptr,
                                        victim, TO_ROOM);
                                    break;
                                case 2:
                                    if (rand_number(1, 4) == 4 && ROOM_EFFECT(IN_ROOM(victim)) == 0) {
                                        ROOM_EFFECT(IN_ROOM(victim)) = 1;
                                        act("Lava leaks up through cracks in the crater!", true, user, nullptr, victim,
                                            TO_CHAR);
                                        act("Lava leaks up through cracks in the crater!", true, user, nullptr, victim,
                                            TO_ROOM);
                                    }
                                    break;
                                case 3:
                                    act("A cloud of dust envelopes the entire area!", true, user, nullptr, victim, TO_CHAR);
                                    act("A cloud of dust envelopes the entire area!", true, user, nullptr, victim, TO_ROOM);
                                    break;
                                case 4:
                                    act("The surrounding area roars and shudders from the impact!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("The surrounding area roars and shudders from the impact!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 5:
                                    act("The ground shatters apart from the stress of the impact!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("The ground shatters apart from the stress of the impact!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 6:
                                    /* One less message */
                                    break;
                                default:
                                    /* we want no message for the default */
                                    break;
                            }
                        }
                        if (SECT(IN_ROOM(victim)) == SECT_UNDERWATER) {
                            switch (rand_number(1, 3)) {
                                case 1:
                                    act("The water churns violently!", true, user, nullptr, victim, TO_CHAR);
                                    act("The water churns violently!", true, user, nullptr, victim, TO_ROOM);
                                    break;
                                case 2:
                                    act("Large bubbles rise from the movement!", true, user, nullptr, victim, TO_CHAR);
                                    act("Large bubbles rise from the movement!", true, user, nullptr, victim, TO_ROOM);
                                    break;
                                case 3:
                                    act("The water collapses in on the hole created!", true, user, nullptr, victim,
                                        TO_CHAR);
                                    act("The water collapses in on the hole create!", true, user, nullptr, victim, TO_ROOM);
                                    break;
                            }
                        }
                        if (SECT(IN_ROOM(victim)) == SECT_WATER_SWIM || SECT(IN_ROOM(victim)) == SECT_WATER_NOSWIM) {
                            switch (rand_number(1, 3)) {
                                case 1:
                                    act("A huge column of water erupts from the impact!", true, user, nullptr, victim,
                                        TO_CHAR);
                                    act("A huge column of water erupts from the impact!", true, user, nullptr, victim,
                                        TO_ROOM);
                                    break;
                                case 2:
                                    act("The impact briefly causes a swirling vortex of water!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("The impact briefly causes a swirling vortex of water!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 3:
                                    act("A huge depression forms in the water and erupts into a wave from the impact!",
                                        true, user, nullptr, victim, TO_CHAR);
                                    act("A huge depression forms in the water and erupts into a wave from the impact!",
                                        true, user, nullptr, victim, TO_ROOM);
                                    break;
                            }
                        }
                        if (SECT(IN_ROOM(victim)) == SECT_INSIDE) {
                            impact_sound(user, "@wA loud roar is heard nearby!@n\r\n");
                            switch (rand_number(1, 8)) {
                                case 1:
                                    act("Debris is thrown into the air and showers down thunderously!", true, user,
                                        nullptr,
                                        victim, TO_CHAR);
                                    act("Debris is thrown into the air and showers down thunderously!", true, user,
                                        nullptr,
                                        victim, TO_ROOM);
                                    break;
                                case 2:
                                    act("The structure of the surrounding room cracks and quakes from the impact!",
                                        true, user, nullptr, victim, TO_CHAR);
                                    act("The structure of the surrounding room cracks and quakes from the impact!",
                                        true, user, nullptr, victim, TO_ROOM);
                                    break;
                                case 3:
                                    act("Parts of the ceiling collapse, crushing into the floor!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("Parts of the ceiling collapse, crushing into the floor!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 4:
                                    act("The surrounding area roars and shudders from the impact!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("The surrounding area roars and shudders from the impact!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 5:
                                    act("The ground shatters apart from the stress of the impact!", true, user, nullptr,
                                        victim,
                                        TO_CHAR);
                                    act("The ground shatters apart from the stress of the impact!", true, user, nullptr,
                                        victim,
                                        TO_ROOM);
                                    break;
                                case 6:
                                    act("The walls of the surrounding room crack in the same instant!", true, user,
                                        nullptr,
                                        victim, TO_CHAR);
                                    act("The walls of the surrounding room crack in the same instant!", true, user,
                                        nullptr,
                                        victim, TO_ROOM);
                                    break;
                                default:
                                    /* we want no message for the default */
                                    break;
                        }
                    }
                }
        
                break;
            case 3:
                actUser("@WYou fly at @C$N@W, slamming both your fists into $S gut as you fly!@n");
                actVictim("@C$n@W flies at you, slamming both $s fists into your gut as $e flies!@n");
                actOthers("@c$n@W flies at @C$N@W, slamming both $s fists into $S gut as $e flies!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou slam both your fists into @C$N@W, hitting $M in the arm!@n");
                actVictim("@C$n@W slams both $s fists into you, hitting you in the arm!@n");
                actOthers("@c$n@W slams both $s fists into @C$N@W, hitting $M in the arm!@n");
                break;
            case 5: /* Weak 2 */
                actUser("@WYou slam both your fists into @C$N's@W leg!@n");
                actVictim("@C$n@W slams both $s fists into your leg!@n");
                actOthers("@c$n@W slams both $s fists into @C$N's@W leg!@n");
                break;
        }
    }

    void Slam::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= calc_critical(user, 0);
                if (!AFF_FLAGGED(victim, AFF_KNOCKED) &&
                        (rand_number(1, 8) >= 7 && (GET_HIT(victim) > GET_HIT(user) / 5) &&
                         !AFF_FLAGGED(victim, AFF_SANCTUARY))) {
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@WYou are knocked out!@n", true, user, nullptr, victim, TO_VICT);
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->setStatusKnockedOut();
                } else if ((GET_POS(victim) == POS_STANDING || GET_POS(victim) == POS_FIGHTING) &&
                               !AFF_FLAGGED(victim, AFF_KNOCKED)) {
                    GET_POS(victim) = POS_SITTING;
                }
                victim->getRoom()->modDamage(5);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }


    // Heeldrop
    std::optional<int> Heeldrop::hasCooldown() {
        if(IS_PICCOLO(user) && GET_SKILL_BASE(user, SKILL_STYLE) >= 75) 
            return 5;
        else
            return 7;
    }

    void Heeldrop::announceObject() {
        actUser("@WYou heeldrop $p@W as hard as you can!@n");
        actRoom("@C$n@W heeldrops $p@W extremely hard!@n");
    }

    void Heeldrop::handleHitspot() {
        switch (hitspot) {
            case 1:
                tech_handle_crashdown(user, victim);
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                tech_handle_crashdown(user, victim);
                calcDamage *= calc_critical(user, 0);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
        
    }

    void Heeldrop::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou disappear, appearing above @C$N@W you spin and heeldrop $M in the face!@n");
                actVictim("@C$n@W disappears, only to appear above you, spinning quickly and heeldropping you in the face!@n");
                actOthers("@c$n@W disappears, only to appear above @C$N@W, spinning quickly and heeldropping $M in the face!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou disappear, reappearing in front of @C$N@W, you flip upside down and slam your heel into the top of $S head!@n");
                actVictim("@C$n@W disappears, reappearing in front of you, $e flips upside down and slams $s heel into the top of your head!@n");
                actOthers("@c$n@W disappears, reappearing in front of @C$N@W, $e flips upside down and slams $s heel into the top of @C$N@W's head!@n");
                break;
            case 3:
                actUser("@WYou fly at @C$N@W, heeldropping $S gut as you fly!@n");
                actVictim("@C$n@W flies at you, heeldropping your gut as $e flies!@n");
                actOthers("@c$n@W flies at @C$N@W, heeldropping $S gut as $e flies!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou heeldrop @C$N@W, hitting $M in the arm!@n");
                actVictim("@C$n@W heeldrops you, hitting you in the arm!@n");
                actOthers("@c$n@W heeldrops @C$N@W, hitting $M in the arm!@n");
                break;
            case 5: /* Weak 2 */
                actUser("@WYou heeldrop @C$N's@W leg!@n");
                actVictim("@C$n@W heeldrops your leg!@n");
                actOthers("@c$n@W heeldrops @C$N's@W leg!@n");
                break;
        }
    }


    // Headbutt
    std::optional<int> Headbutt::hasCooldown() {
        if(IS_KURZAK(user) && GET_SKILL_BASE(user, SKILL_STYLE) >= 100) 
            return 5;
        else
            return 6;
    }

    void Headbutt::announceObject() {
        actUser("@WYou headbutt $p@W as hard as you can!@n");
        actRoom("@C$n@W headbutt $p@W extremely hard!@n");
    }

    void Headbutt::handleHitspot() {
        if (IS_KURZAK(user)) {
            if (GET_SKILL(user, SKILL_HEADBUTT) >= 60) {
                calcDamage += calcDamage * 0.1;
            } else if (GET_SKILL(user, SKILL_HEADBUTT) >= 40) {
                calcDamage += calcDamage * 0.05;
            }
        }

        int multiplier = 0;
        switch (hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                if (!AFF_FLAGGED(victim, AFF_KNOCKED) &&
                        (rand_number(1, 7) >= 4 && (GET_HIT(victim) > GET_HIT(user) / 5) &&
                         !AFF_FLAGGED(victim, AFF_SANCTUARY))) {
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@WYou are knocked out!@n", true, user, nullptr, victim, TO_VICT);
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->setStatusKnockedOut();
                }
                
                if (IS_KURZAK(user) && !IS_NPC(user)) {
                    if (GET_SKILL_BASE(user, SKILL_STYLE) >= 75)
                        multiplier += 1;
                }
                calcDamage *= (calc_critical(user, 0) + multiplier);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
        
    }

    void Headbutt::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou grab @c$N@W by the shoulders and slam your head into $S chest!@n");
                actVictim("@C$n@W grabs YOU by the shoulders and slams $s head into YOUR chest!@n");
                actOthers("@C$n@W grabs @c$N@W by the shoulders and slams $s head into @c$N's@W chest!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou grab @c$N@W by the shoulders and slam your head into $S face!@n");
                actVictim("@C$n@W grabs YOU by the shoulders and slams $s head into YOUR face!@n");
                actOthers("@C$n@W grabs @c$N@W by the shoulders and slams $s head into @c$N's@W face!@n");
                break;
            case 3:
                actUser("@WYou grab @c$N@W by the shoulders and slam your head into $S chest!@n");
                actVictim("@C$n@W grabs YOU by the shoulders and slams $s head into YOUR chest!@n");
                actOthers("@C$n@W grabs @c$N@W by the shoulders and slams $s head into @c$N's@W chest!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou grab @c$N@W and barely manage to slam your head into $S leg!@n");
                actVictim("@C$n@W grabs YOU and barely manages to slam $s head into YOUR leg!@n");
                actOthers("@C$n@W grabs @c$N@W and barely manages to slam $s head into @c$N's@W leg!@n");
                break;
            case 5: /* Weak 2 */
                actUser("@WYou grab @c$N@W and barely manage to slam your head into $S arm!@n");
                actVictim("@C$n@W grabs YOU and barely manages to slam $s head into YOUR arm!@n");
                actOthers("@C$n@W grabs @c$N@W and barely manages to slam $s head into @c$N's@W arm!@n");
                break;
        }
    }



    // KI
    bool RangedKiAttack::checkOtherConditions() {
        if(!args.empty() && args.size() >= 2) {
            if ((char*)args[1].c_str()) {
                double adjust = (double)(atoi((char*)args[1].c_str())) * 0.01;
                if (adjust < 0.01 || adjust > 1.00) {
                    send_to_char(user, "If you are going to supply a percentage of your charge to use then use an acceptable number (1-100)\r\n");
                    return false;
                }
            
            }
        }
        return true;
    }

    double RangedKiAttack::calculateKiCost() {
        double minimum = 0.01; 
        attPerc = 0.05;

        if (getTier() == 1) {
            minimum = 0.01;
            attPerc = 0.05;
        }
        if (getTier() == 2) {
            minimum = 0.05;
            attPerc = 0.10;
        }
        if (getTier() == 3) {
            minimum = 0.1;
            attPerc = 0.2;
        }
        if (getTier() == 4) {
            minimum = 0.2;
            attPerc = 0.35;
        }
        if (getTier() == 5) {
            minimum = 0.35;
            attPerc = 0.50;
        }


        if(!args.empty() && args.size() >= 2) {
            bool success = tech_handle_charge(user, (char*)args[1].c_str(), minimum, &attPerc);
        }


        return attPerc * getKiEfficiency();
    }

    double RangedKiAttack::getKiEfficiency() {
        return 1;
    }

    Result RangedKiAttack::handleParry() {
        announceParry();
        defenseResult = DefenseResult::Parried;
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        improve_skill(victim, SKILL_PARRY, 0);
        parry_ki(attPerc, user, victim, (char*)getName().c_str(), 0, 0, getSkillID(), getAtkID());
        user->getRoom()->modDamage(5);
        return Result::Missed;

    }

    Result RangedKiAttack::handleDodge() {
        announceDodge();
        defenseResult = DefenseResult::Dodged;
        improve_skill(victim, SKILL_DODGE, 0);
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        if(AFF_FLAGGED(user, AFF_ZANZOKEN))
            pcost(victim, GET_MAX_MANA(victim) * 0.05, 0);

        dodge_ki(user, victim, getHoming(), getAtkID(), initSkill, getSkillID());
        user->getRoom()->modDamage(5 * getTier());
        return Result::Missed;

    }

    Result RangedKiAttack::handlePerfectDodge() {
        actVictim("@C$n@W moves so slowly that you dodge their attack with ease.@n");
        actUser("@WYou move quickly and yet @C$N@W simply sidesteps you!@n");
        actOthers("@C$n@W moves quickly and yet @c$N@W dodges with ease!@n");

        if(victim->getCurKI() > 0) {
            victim->decCurKI(calcDamage * 1 + victim->getAffectModifier(APPLY_PERFECT_DODGE));
        } else {
            victim->decCurST(2 * calcDamage * 1 + victim->getAffectModifier(APPLY_PERFECT_DODGE));
            actVictim("@WContinuing to dodge without Ki takes a heavy toll.@n");
        }

        return Result::Missed;
    }

    Result RangedKiAttack::handleBlock() {
        if (tech_handle_android_absorb(user, victim)) {
            pcost(user, 1, 0);
            return Result::Missed;
        }
        announceBlock();
        defenseResult = DefenseResult::Blocked;
        improve_skill(victim, SKILL_BLOCK, 0);
        pcost(victim, 0, GET_MAX_HIT(victim) / 500);
        return Result::Landed;

    }

    

    void RangedKiAttack::announceObject() {
        actUser("@WYou fire a " + getName() + " at $p@W!@n");
        actRoom("@C$n@W fires a " + getName() + " at $p@W!@n");
    }

    void RangedKiAttack::announceParry() {
        actUser("@C$N@W deflects your " + getName() + ", sending it flying away!@n");
        actVictim("@WYou deflect @C$n's@W " + getName() + " sending it flying away!@n");
        actOthers("@C$N@W deflects @c$n's@W " + getName() + " sending it flying away!@n");
    }

    void RangedKiAttack::announceBlock() {
        actUser("@C$N@W moves quickly and blocks " + getName() + "!@n");
        actVictim("@WYou move quickly and block @C$n's@W " + getName() + "!@n");
        actOthers("@C$N@W moves quickly and blocks @c$n's@W " + getName() + "!@n");
    }

    void RangedKiAttack::announceDodge() {
        actUser("@C$N@W manages to dodge your " + getName() + ", letting it slam into the surroundings!@n");
        actVictim("@WYou dodge @C$n's@W " + getName() + ", letting it slam into the surroundings!@n");
        actOthers("@C$N@W manages to dodge @c$n's@W " + getName() + ", letting it slam into the surroundings!@n");
    }

    void RangedKiAttack::announceMiss() {
        actUser("@WYou can't believe it but your " + getName() + " misses, flying through the air harmlessly!@n");
        actVictim("@C$n@W fires a " + getName() + " at you, but misses!@n");
        actOthers("@c$n@W fires a " + getName() + " at @C$N@W, but somehow misses!@n");
    }

    


    // Kiball
    void KiBall::processAttack() {
        int mult_roll = rand_number(1, 100), mult_count = 1, mult_chance = 0;

        if (initSkill >= 100) {
            mult_chance = 30;
        } else if (initSkill >= 75) {
            mult_chance = 15;
        } else if (initSkill >= 50) {
            mult_chance = 10;
        }

        if (mult_roll <= mult_chance)
            mult_count = rand_number(2, 3);

        while (mult_count > 0) {
            mult_count -= 1;
            switch(doAttack()) {
                case Result::Landed:
                    onLanded();
                    onLandedOrMissed();
                    break;
                case Result::Missed:
                    onMissed();
                    onLandedOrMissed();
                    break;
                case Result::Canceled:
                    onCanceled();
                    break;
            }
        }
    }

    void KiBall::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou hold out your hand towards @C$N@W, and fire a bright yellow kiball! The kiball slams into $M quickly and explodes with roaring light!@n");
                actVictim("@c$n@W holds out $s hand towards you, and fires a bright yellow kiball! The kiball slams into you quickly and explodes with roaring light!@n");
                actOthers("@c$n@W holds out $s hand towards @C$N@W, and fires a bright yellow kiball! The kiball slams into $M quickly and explodes with roaring light!@n");
                break;
            case 2:
                actUser("@WYou hold out your hand towards @C$N@W, and fire a bright yellow kiball! The kiball slams into $S face and explodes, shrouding $S head with smoke!@n");
                actVictim("@c$n@W holds out $s hand towards you, and fires a bright yellow kiball! The kiball slams into your face and explodes, leaving you choking on smoke!@n");
                actOthers("@c$n@W holds out $s hand towards @C$N@W, and fires a bright yellow kiball! The kiball slams into $S face and explodes, shrouding $S head with smoke!@n");
                break;
            case 3:
                actUser("@WYou hold out your hand towards @C$N@W, and fire a bright yellow kiball! The kiball slams into $S body and explodes with a loud roar!@n");
                actVictim("@c$n@W holds out $s hand towards you, and fires a bright yellow kiball! The kiball slams into your body and explodes with a loud roar!@n");
                actOthers("@c$n@W holds out $s hand towards @C$N@W, and fires a bright yellow kiball! The kiball slams into $S body and explodes with a loud roar!@n");
                break;
            case 4:
                actUser("@WYou hold out your hand towards @C$N@W, and fire a bright yellow kiball! The kiball grazes $S arm and explodes shortly after!@n");
                actVictim("@c$n@W holds out $s hand towards you, and fires a bright yellow kiball! The kiball grazes your arm and explodes shortly after!@n");
                actOthers("@c$n@W holds out $s hand towards @C$N@W, and fires a bright yellow kiball! The kiball grazes $S arm and explodes shortly after!@n");
                break;
            case 5:
                actUser("@WYou hold out your hand towards @C$N@W, and fire a bright yellow kiball! The kiball grazes $S leg and explodes shortly after!@n");
                actVictim("@c$n@W holds out $s hand towards you, and fires a bright yellow kiball! The kiball grazes your leg and explodes shortly after!@n");
                actOthers("@c$n@W holds out $s hand towards @C$N@W, and fires a bright yellow kiball! The kiball grazes $S leg and explodes shortly after!@n");
                break;
        }
    }

    //KiBlast
    std::optional<int> KiBlast::hasCooldown() {
        if (!IS_ANDROID(user) || initSkill < 100)
            handle_cooldown(user, 5);

        return 1;
    }

    void KiBlast::attackPreprocess() {
        record = GET_HIT(victim);
        if (IS_ANDROID(user)) {
                if (GET_SKILL(user, SKILL_KIBLAST) >= 100) {
                    calcDamage += calcDamage * 0.15;
                } else if (GET_SKILL(user, SKILL_KIBLAST) >= 60) {
                    calcDamage += calcDamage * 0.10;
                } else if (GET_SKILL(user, SKILL_KIBLAST) >= 40) {
                    calcDamage += calcDamage * 0.05;
                }
        }
    }

    void KiBlast::attackPostprocess() {
        int mastery = rand_number(1, 100), master_pass = false, chance = 0;

        if (initSkill >= 100)
            chance = 30;
        else if (initSkill >= 75)
            chance = 20;
        else if (initSkill >= 50)
            chance = 15;

        if (mastery <= chance)
            master_pass = true;

        if (master_pass == true && record > GET_HIT(victim) &&
            (record - GET_HIT(victim) > (victim->getMaxPL()) * 0.025)) {
            if (!AFF_FLAGGED(victim, AFF_KNOCKED) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_CHAR);
                act("@WYou are knocked out!@n", true, user, nullptr, victim, TO_VICT);
                act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_NOTVICT);
                 victim->setStatusKnockedOut();
            }
        }
    }

    void KiBlast::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large kiblast that slams into $S chest!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into your chest!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into $N@W's chest!@n");
                break;
            case 2:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large kiblast that slams into $S face!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into your face!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into $N@W's face!@n");
                break;
            case 3:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large kiblast that slams into $S gut!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into your gut!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into $N@W's gut!@n");
                break;
            case 4:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large kiblast that slams into $S arm!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into your arm!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into $N@W's arm!@n");
                break;
            case 5:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large kiblast that slams into $S leg!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into your leg!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large kiblast that slams into $N@W's leg!@n");
                break;
        }
    }


    // Beam
    void Beam::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large beam that slams into $S chest!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into your chest!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into $N@W's chest!@n");
                break;
            case 2:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large beam that slams into $S face!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into your face!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into $N@W's face!@n");
                break;
            case 3:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large beam that slams into $S gut!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into your gut!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into $N@W's gut!@n");
                break;
            case 4:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large beam that slams into $S arm!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into your arm!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into $N@W's arm!@n");
                break;
            case 5:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large beam that slams into $S leg!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into your leg!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large beam that slams into $N@W's leg!@n");
                break;
        }
    }

    void Beam::attackPostprocess() {
        int master_roll = rand_number(1, 100), master_chance = 0, master_pass = false;

        if (initSkill >= 100)
            master_chance = 20;
        else if (initSkill >= 75)
            master_chance = 10;
        else if (initSkill >= 50)
            master_chance = 5;

        if (master_chance >= master_roll)
            master_pass = true;

        if (GET_HIT(victim) > 0 && calcDamage > GET_MAX_HIT(victim) / 4 && master_pass == true) {
            int attempt = rand_number(0, NUM_OF_DIRS);  /* Select a random direction */
            int count = 0;
            while (count < 12) {
                attempt = count;
                if (CAN_GO(victim, attempt)) {
                    count = 12;
                } else {
                    count++;
                }
            }
            if (CAN_GO(victim, attempt)) {
                act("$N@W is pushed away by the blast!@n", true, user, nullptr, victim, TO_CHAR);
                act("@WYou are pushed away by the blast!@n", true, user, nullptr, victim, TO_VICT);
                act("$N@W is pushed away by the blast!@n", true, user, nullptr, victim, TO_NOTVICT);
                do_simple_move(victim, attempt, true);
            } else {
                act("$N@W is pushed away by the blast, but is slammed into an obstruction!@n", true, user, nullptr,
                    victim,
                    TO_CHAR);
                act("@WYou are pushed away by the blast, but are slammed into an obstruction!@n", true, user, nullptr,
                    victim,
                    TO_VICT);
                act("$N@W is pushed away by the blast, but is slammed into an obstruction!@n", true, user, nullptr,
                    victim,
                    TO_NOTVICT);
                calcDamage *= 2;
                hurt(1, 195, user, victim, nullptr, calcDamage, 1);
            }
        }
    }


    // Tsuihidan
    void Tsuihidan::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large tsuihidan that slams into $s chest!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into your chest!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into $N@W's chest!@n");
                break;
            case 2:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large tsuihidan that slams into $S face!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into your face!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into $N@W's face!@n");
                break;
            case 3:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large tsuihidan that slams into $S gut!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into your gut!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into $N@W's gut!@n");
                break;
            case 4:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large tsuihidan that slams into $S arm!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into your arm!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into $N@W's arm!@n");
                break;
            case 5:
                actUser("@WYou aim your hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly you unleash a large tsuihidan that slams into $S leg!@n");
                actVictim("@W$n@W aims $s hand at you, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into your leg!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and bright @Yyellow@W energy begins to pool there. Suddenly $e unleashes a large tsuihidan that slams into $N@W's leg!@n");
                break;
        }
    }

    void Tsuihidan::attackPostprocess() {
        int master_roll = rand_number(1, 100), master_chance = 0, master_pass = false;

        if (initSkill >= 100)
            master_chance = 20;
        else if (initSkill >= 75)
            master_chance = 10;
        else if (initSkill >= 50)
            master_chance = 5;

        if (master_chance >= master_roll)
            master_pass = true;

        if (master_pass == true) {
            victim->decCurST(calcDamage);
            act("@CYour tsuihidan hits a vital spot and seems to sap some of @c$N's@C stamina!@n", true, user,
                nullptr, victim, TO_CHAR);
            act("@C$n's@C tsuihidan hits a vital spot and saps some of your stamina!@n", true, user, nullptr, victim,
                TO_VICT);
            act("@C$n's@C tsuihidan hits a vital spot and saps some of @c$N's@C stamina!", true, user, nullptr, victim,
                TO_NOTVICT);
        }
    }

    // Renzo

    DefenseResult Renzo::calculateDefense() {
        count = 100;

        if (IS_NAIL(user)) {
            if (initSkill >= 100) {
                count += 200;
            } else if (initSkill >= 60) {
                count += 100;
            } else if (initSkill >= 40) {
                count += 40;
            }
        }
        if (rand_number(1, 5) >= 5) { /* Random boost or neg for everyone */
            count += rand_number(-15, 25);
        }



        double dodgeChance = ((double) currentDodgeCheck / 5.0) * ((double) axion_dice(0) / 120.0) * (1.0 + victim->getAffectModifier(APPLY_DODGE_PERC));
        double blockChance = ((double) currentBlockCheck / 3.0) * ((double) axion_dice(0) / 120.0) * (1.0 + victim->getAffectModifier(APPLY_BLOCK_PERC));

        double overcomeDodge = (double) currentChanceToHit * ((double) axion_dice(0) / 120.0);
        double overcomeBlock = (double) currentChanceToHit * ((double) axion_dice(0) / 120.0);
        

        if(canParry() && calculateDeflect()) count -= axion_dice(0);
        if(count <= 0) return DefenseResult::Parried;
        if(canDodge() && dodgeChance > overcomeDodge) count -= axion_dice(0);
        if(count <= 0) return DefenseResult::Dodged;
        if(canBlock() && blockChance > overcomeBlock) return DefenseResult::Blocked;

        return DefenseResult::Failed;
    }

    double Renzo::getKiEfficiency() {
        int master_roll = rand_number(1, 100), master_chance = 0, half_chance = 0, master_pass = 0;

        if (initSkill >= 100) {
            master_chance = 10;
            half_chance = 20;
        } else if (initSkill >= 75) {
            master_chance = 5;
            half_chance = 10;
        } else if (initSkill >= 50) {
            master_chance = 5;
            half_chance = 5;
        }

        if (master_chance >= master_roll)
            master_pass = 1;
        else if (half_chance >= master_roll)
            master_pass = 2;

        if (master_pass == 1) {
            send_to_char(user, "@GYour mastery of the technique has made your use of energy more efficient!@n\r\n");
            return 0.25;
        }
        else if (master_pass == 2) {
            send_to_char(user,
                "@GYour mastery of the technique has made your use of energy as efficient as possible!@n\r\n");
            return 0.5;
        }

        return 1.0;
    }

    void Renzo::attackPreprocess() {
        calcDamage = calcDamage * 0.01;
        calcDamage *= count;
        
    }

    void Renzo::handleHitspot() {
        hitspot = 1;
    }

    void Renzo::announceHitspot() {
        if (count >= 100) {
            actUser("@WYou gather your charged energy into your hands as a golden glow appears around each. You slam your hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! All of the shots hit!");
            actVictim("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at you! All of the shots hit!");
            actOthers("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! All of the shots hit!");
        }
        if (count >= 75 && count < 100) {
            actUser("@WYou gather your charged energy into your hands as a golden glow appears around each. You slam your hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Most of the shots hit, but some of them are avoided by $M!");
            actVictim("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at you! Most of the shots hit, but some of them you manage to avoid!");
            actOthers("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Most of the shots hit, but some of them are avoided by $M!");
        }
        if (count >= 50 && count < 75) {
            actUser("@WYou gather your charged energy into your hands as a golden glow appears around each. You slam your hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! About half of the shots hit, the rest are avoided by $M!");
            actVictim("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at you! About half of the shots hit, the rest you manage to avoid!");
            actOthers("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! About half of the shots hit, the rest are avoided by $M!");
        }
        if (count >= 25 && count < 50) {
            actUser("@WYou gather your charged energy into your hands as a golden glow appears around each. You slam your hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Few of the shots hit, the rest are avoided by $M!");
            actVictim("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at you! Few of the shots hit, the rest you manage to avoid!");
            actOthers("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Few of the shots hit, the rest are avoided by $M!");
        }
        if (count < 25) {
            actUser("@WYou gather your charged energy into your hands as a golden glow appears around each. You slam your hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Very few of the shots hit, the rest are avoided by $M!");
            actVictim("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at you! Very few of the shots hit, the rest you manage to avoid!");
            actOthers("@w$n gathers charged energy into $s hands as a golden glow appears around each. $e slams $s hands forward rapidly firing hundreds of Renzokou Energy Dan shots at $N@W! Very few of the shots hit, the rest are avoided by $M!");
        }
        
    }


    // Shogekiha
    std::optional<int> Shogekiha::hasCooldown() {
         if (!IS_KABITO(user) || initSkill < 100) 
            return 5;
        
        return 1;    
    }

    void Shogekiha::attackPostprocess() {
        int master_roll = rand_number(1, 100), master_chance = 0, master_pass = false;

        if (initSkill >= 100)
            master_chance = 20;
        else if (initSkill >= 75)
            master_chance = 10;
        else if (initSkill >= 50)
            master_chance = 5;

        if (master_chance >= master_roll)
            master_pass = true;

        if (master_pass == true) {
            act("@CYour skillful shogekiha dissipated some of @c$N's@C charged ki!@n", true, user, nullptr, victim,
                TO_CHAR);
            act("@C$n@C's skillful shogekiha dissipated some of YOUR charged ki!@n", true, user, nullptr, victim,
                TO_VICT);
            act("@C$n@C's skillful shogekiha dissipated some of @c$N's@C charged ki!", true, user, nullptr, victim,
                TO_NOTVICT);
            GET_CHARGE(victim) -= GET_CHARGE(victim) * 0.25;
        }
    }


    void Shogekiha::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou aim your hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly you unleash a large shogekiha that slams into $S chest!@n");
                actVictim("@W$n@W aims $s hand at you, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into your chest!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into $N@W's chest!@n");
                break;
            case 2:
                actUser("@WYou aim your hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly you unleash a large shogekiha that slams into $S face!@n");
                actVictim("@W$n@W aims $s hand at you, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into your face!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into $N@W's face!@n");
                break;
            case 3:
                actUser("@WYou aim your hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly you unleash a large shogekiha that slams into $S gut!@n");
                actVictim("@W$n@W aims $s hand at you, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into your gut!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into $N@W's gut!@n");
                break;
            case 4:
                actUser("@WYou aim your hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly you unleash a large shogekiha that slams into $S arm!@n");
                actVictim("@W$n@W aims $s hand at you, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into your arm!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into $N@W's arm!@n");
                break;
            case 5:
                actUser("@WYou aim your hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly you unleash a large shogekiha that slams into $S leg!@n");
                actVictim("@W$n@W aims $s hand at you, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into your leg!@n");
                actOthers("@W$n@W aims $s hand at $N@W, and nearby loose objects begin to be pushed out by an invisible force. Suddenly $e unleashes a large shogekiha that slams into $N@W's leg!@n");
                break;
        }
    }

    Result Shogekiha::attackObject() {
        if(!can_kill(user, nullptr, obj, canKillType())) return Result::Canceled;
        bool extracted = false;
        calcDamage = calculateObjectDamage();

        if (KICHARGE(obj) > 0 && GET_CHARGE(user) > KICHARGE(obj)) {
            KICHARGE(obj) -= GET_CHARGE(user);
            extract_obj(obj);
            extracted = true;
        } else if (KICHARGE(obj) > 0 && GET_CHARGE(user) < KICHARGE(obj)) {
            KICHARGE(obj) -= GET_CHARGE(user);
            GET_CHARGE(user) = 0;
            calcDamage = KICHARGE(obj);
            hurt(0, 0, USER(obj), user, nullptr, calcDamage, 0);
            extract_obj(obj);
            extracted = true;
        }

        announceObject();
        if(extracted) return Result::Canceled;
        return Result::Landed;
    }

    void Shogekiha::announceObject() {
        if (KICHARGE(obj) > 0 && GET_CHARGE(user) > KICHARGE(obj)) {
            actUser("@WYou leap at $p@W with your arms spread out to your sides. As you are about to make contact with $p@W you scream and shatter the attack with your ki!@n");
            actRoom("@C$n@W leaps out at $p@W with $s arms spead out to $s sides. As $e is about to make contact with $p@W $e screams and shatters the attack with $s ki!@n");
        } else if (KICHARGE(obj) > 0 && GET_CHARGE(user) < KICHARGE(obj)) {
            actUser("@WYou leap at $p@W with your arms spread out to your sides. As you are about to make contact with $p@W you scream and weaken the attack with your ki before taking the rest of the attack in the chest!@n");
            actRoom("@C$n@W leaps out at $p@W with $s arms spead out to $s sides. As $e is about to make contact with $p@W $e screams and weakens the attack with $s ki before taking the rest of the attack in the chest!@n");
        } else {
            actUser("@WYou fire a shogekiha at $p@W!@n");
            actRoom("@C$n@W fires a shogekiha at $p@W!@n");
        }
    }

    //Kahamehameha
    std::optional<int> Kamehameha::hasCooldown() {
        if (GET_SKILL_PERF(user, getSkillID()) == 3) return 3;
        return 6;
    }

    void Kamehameha::postProcess() {
        if (GET_SKILL(user, getSkillID()) >= 100) {
            user->incCurKI((GET_MAX_MANA(user) * attPerc) * 0.25);
        } else if (GET_SKILL(user, getSkillID()) >= 60) {
            user->incCurKI((GET_MAX_MANA(user) * attPerc) * 0.1);
        } else if (GET_SKILL(user, getSkillID()) >= 40) {
            user->incCurKI((GET_MAX_MANA(user) * attPerc) * 0.05);
        }
    }

    void Kamehameha::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou cup your hands at your side and begin to pool your charged ki there. As the ki pools there you begin to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly you bring your hands forward facing @c$N@W and shout '@BHAAAA!!!@W' while releasing a bright blue kamehameha at $M! It slams into $S body and explodes!@n");
                actVictim("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing you and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into your body and explodes!@n");
                actOthers("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing @c$N@W and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into $S body and explodes!@n");
                break;
            case 2:
                actUser("@WYou cup your hands at your side and begin to pool your charged ki there. As the ki pools there you begin to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly you bring your hands forward facing @c$N@W and shout '@BHAAAA!!!@W' while releasing a bright blue kamehameha at $M! It slams into $S face and explodes!@n");
                actVictim("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing you and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into your face and explodes!@n");
                actOthers("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing @c$N@W and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into $S face and explodes!@n");
                break;
            case 3:
                actUser("@WYou cup your hands at your side and begin to pool your charged ki there. As the ki pools there you begin to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly you bring your hands forward facing @c$N@W and shout '@BHAAAA!!!@W' while releasing a bright blue kamehameha at $M! It slams into $S gut and explodes!@n");
                actVictim("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing you and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into your gut and explodes!@n");
                actOthers("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing @c$N@W and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into $S gut and explodes!@n");
                break;
            case 4:
                actUser("@WYou cup your hands at your side and begin to pool your charged ki there. As the ki pools there you begin to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly you bring your hands forward facing @c$N@W and shout '@BHAAAA!!!@W' while releasing a bright blue kamehameha at $M! It slams into $S arm and explodes!@n");
                actVictim("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing you and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into your arm and explodes!@n");
                actOthers("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing @c$N@W and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into $S arm and explodes!@n");
                break;
            case 5:
                actUser("@WYou cup your hands at your side and begin to pool your charged ki there. As the ki pools there you begin to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly you bring your hands forward facing @c$N@W and shout '@BHAAAA!!!@W' while releasing a bright blue kamehameha at $M! It slams into $S leg and explodes!@n");
                actVictim("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing you and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into your leg and explodes!@n");
                actOthers("@C$n@W cups $s hands at $s side and begins to pool charged ki there. As the ki pools there $e begins to chant, '@BKaaaaa@bmeeee@Bhaaaaaaa@bmeeeee@W'. Suddenly $e brings $s hands forward facing @c$N@W and shouts '@BHAAAA!!!@W' while releasing a bright blue kamehameha! It slams into $S leg and explodes!@n");
                break;
        }
    }


    // Masenko

    void Masenko::attackPreprocess() {
        if (initSkill >= 100) {
            calcDamage += calcDamage * 0.08;
        } else if (initSkill >= 60) {
            calcDamage += calcDamage * 0.05;
        } else if (initSkill >= 40) {
            calcDamage += calcDamage * 0.03;
        }
        
        if (IS_PICCOLO(user)) {
            if (GET_SKILL_BASE(user, SKILL_STYLE) >= 100)
                calcDamage += (GET_MAX_MANA(user) * 0.08);
            else if (GET_SKILL_BASE(user, SKILL_STYLE) >= 60)
                calcDamage += (GET_MAX_MANA(user) * 0.05);
            else if (GET_SKILL_BASE(user, SKILL_STYLE) >= 40)
                calcDamage += (GET_MAX_MANA(user) * 0.03);
        }
    }

    void Masenko::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou place your hands on top of each other and raise them over your head. Energy that you have charged gathers into your palms just before you bring them down aimed at @c$N@W! You scream '@RMasenko @rHa!@W' as you release your attack. A bright red wave of energy flows from your hands and slams into $S body!@n");
                actVictim("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at you! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into YOUR body!@n");
                actOthers("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at @c$N@W! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into @c$N@W's body!@n");
                break;
            case 2:
                actUser("@WYou place your hands on top of each other and raise them over your head. Energy that you have charged gathers into your palms just before you bring them down aimed at @c$N@W! You scream '@RMasenko @rHa!@W' as you release your attack. A bright red wave of energy flows from your hands and slams into $S face!@n");
                actVictim("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at you! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into YOUR face!@n");
                actOthers("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at @c$N@W! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into @c$N@W's face!@n");
                break;
            case 3:
                actUser("@WYou place your hands on top of each other and raise them over your head. Energy that you have charged gathers into your palms just before you bring them down aimed at @c$N@W! You scream '@RMasenko @rHa!@W' as you release your attack. A bright red wave of energy flows from your hands and slams into $S gut!@n");
                actVictim("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at you! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into YOUR gut!@n");
                actOthers("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at @c$N@W! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into @c$N@W's gut!@n");
                break;
            case 4:
                actUser("@WYou place your hands on top of each other and raise them over your head. Energy that you have charged gathers into your palms just before you bring them down aimed at @c$N@W! You scream '@RMasenko @rHa!@W' as you release your attack. A bright red wave of energy flows from your hands and slams into $S arm!@n");
                actVictim("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at you! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into YOUR arm!@n");
                actOthers("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at @c$N@W! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into @c$N@W's arm!@n");
                break;
            case 5:
                actUser("@WYou place your hands on top of each other and raise them over your head. Energy that you have charged gathers into your palms just before you bring them down aimed at @c$N@W! You scream '@RMasenko @rHa!@W' as you release your attack. A bright red wave of energy flows from your hands and slams into $S leg!@n");
                actVictim("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at you! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into YOUR leg!@n");
                actOthers("@C$n@W places $s hands on top of each other and raises them over $s head. Energy that $e had charged gathers into $s palms just before $e brings them down aimed at @c$N@W! $e screams '@RMasenko @rHa!@W' as $e releases $s attack. A bright red wave of energy flows from $s hands and slams into @c$N@W's leg!@n");
                break;
        }
    }

    // Dodonpa
    void Dodonpa::attackPostprocess() {
        if (rand_number(1, 3) == 2) {
            victim->decCurKI(calcDamage / 4);
            send_to_char(victim, "@RYou feel some of your ki drained away by the attack!@n\r\n");
        }
    }

    void Dodonpa::announceHitspot() {
        switch(hitspot) {
            case 1:
                actUser("@WYou gather your charged ki into your index finger and point at @c$N@W. A @Ygolden@W glow forms around your finger tip right before you shout '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fire a large golden beam! The beam slams into $S body and explodes!@n");
                actVictim("@C$n@W gathers $s charged ki into $s index finger and points at you. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into your body and explodes!@n");
                actOthers("@C$n@W gathers $s charged ki into $s index finger and points at @c$N@W. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into @c$N@W's body and explodes!@n");
                break;
            case 2:
                actUser("@WYou gather your charged ki into your index finger and point at @c$N@W. A @Ygolden@W glow forms around your finger tip right before you shout '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fire a large golden beam! The beam slams into $S face and explodes!@n");
                actVictim("@C$n@W gathers $s charged ki into $s index finger and points at you. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into your face and explodes!@n");
                actOthers("@C$n@W gathers $s charged ki into $s index finger and points at @c$N@W. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into @c$N@W's face and explodes!@n");
                break;
            case 3:
                actUser("@WYou gather your charged ki into your index finger and point at @c$N@W. A @Ygolden@W glow forms around your finger tip right before you shout '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fire a large golden beam! The beam slams into $S gut and explodes!@n");
                actVictim("@C$n@W gathers $s charged ki into $s index finger and points at you. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into your gut and explodes!@n");
                actOthers("@C$n@W gathers $s charged ki into $s index finger and points at @c$N@W. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into @c$N@W's gut and explodes!@n");
                break;
            case 4:
                actUser("@WYou gather your charged ki into your index finger and point at @c$N@W. A @Ygolden@W glow forms around your finger tip right before you shout '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fire a large golden beam! The beam slams into $S arm and explodes!@n");
                actVictim("@C$n@W gathers $s charged ki into $s index finger and points at you. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into your arm and explodes!@n");
                actOthers("@C$n@W gathers $s charged ki into $s index finger and points at @c$N@W. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into @c$N@W's arm and explodes!@n");
                break;
            case 5:
                actUser("@WYou gather your charged ki into your index finger and point at @c$N@W. A @Ygolden@W glow forms around your finger tip right before you shout '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fire a large golden beam! The beam slams into $S leg and explodes!@n");
                actVictim("@C$n@W gathers $s charged ki into $s index finger and points at you. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into your leg and explodes!@n");
                actOthers("@C$n@W gathers $s charged ki into $s index finger and points at @c$N@W. A @Ygolden@W glow forms around the finger tip right before $e shouts '@YD@yo@Yd@yo@Yn@yp@Ya@W!' and fires a large golden beam! The beam slams into @c$N@W's leg and explodes!@n");
                break;
        }
    }


    // Galik Gun
    void GalikGun::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou take your charged ki and form a sparkling purple shroud of energy around your body! You swing your arms towards @c$N@W with your palms flatly facing $M and shout '@mG@Mal@wik @mG@Mu@wn@W!' as the energy around your body forms into a beam and crashes into $S " + loc + "!@n");
        actVictim("@C$n@W takes $s charged ki and forms a sparkling purple shroud of energy around $mself! $e swings $s arms towards you with palms facing out flatly and shouts '@mG@Mal@wik @mG@Mu@wn@W!' as the energy around $s body forms into a beam and crashes into your " + loc + "!@n");
        actOthers("@C$n@W takes $s charged ki and forms a sparkling purple shroud of energy around $mself! $e swings $s arms towards @c$N@W with palms facing out flatly and shouts '@mG@Mal@wik @mG@Mu@wn@W!' as the energy around $s body forms into a beam and crashes into @c$N@W's " + loc + "!@n");   
    }


    // DeathBeam
    std::optional<int> DeathBeam::hasCooldown() {
        if (GET_SKILL_PERF(user, getSkillID()) == 3) return 3;
        return 6;
    }

    void DeathBeam::attackPostprocess() {
        if (initSkill >= 100 && GET_HIT(victim) >= 2) {
            victim->decCurLF(calcDamage * .4, -1);
        } else if (initSkill >= 60 && GET_HIT(victim) >= 2) {
            victim->decCurLF(calcDamage * .2, -1);
        } else if (initSkill >= 40 && GET_HIT(victim) >= 2) {
            victim->decCurLF(calcDamage * .05, -1);
        }
    }

    void DeathBeam::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou move swiftly, drawing your charged ki to your index finger, and point at @c$N@W! You fire a @Rred@W Deathbeam from your finger which slams into $S " + loc + " and explodes!@n");
        actVictim("@C$n@W moves swiftly, drawing charged ki to $s index finger, and point at you@W! $e fires a @Rred@W Deathbeam from $s finger which slams into your " + loc + " and explodes!@n");
        actOthers("@C$n@W moves swiftly, drawing charged ki to $s index finger, and point at @c$N@W! $e fires a @Rred@W Deathbeam from $s finger which slams into @c$N@W's " + loc + " and explodes!@n");   
    }


    // EraserCannon
    void EraserCannon::postProcess() {
        if (initSkill >= 100) {
            user->decCurKI((GET_MAX_MANA(user) * attPerc) * 0.15);
        } else if (initSkill >= 60) {
            user->decCurKI((GET_MAX_MANA(user) * attPerc) * 0.1);
        } else if (initSkill >= 40) {
            user->decCurKI((GET_MAX_MANA(user) * attPerc) * 0.05);
        }
    }

    void EraserCannon::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
            case 3:
                calcDamage *= calc_critical(user, 0);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void EraserCannon::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
            case 3:
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou face @c$N@W quickly and open your mouth wide. A @Yg@yo@Yl@yd@Ye@yn@W glow forms from deep in your throat, growing brighter as it rises up and out your mouth. Suddenly a powerful Eraser Cannon erupts and slams into @c$N@W's " + loc + "!@n");
        actVictim("@C$n @Wfaces you quickly and opens $s mouth wide. A @Yg@yo@Yl@yd@Ye@yn@W glow forms from deep in $s throat, growing brighter as it rises up and out $s mouth. Suddenly a powerful Eraser Cannon erupts and slams into your " + loc + "!@n");
        actOthers("@C$n @Wfaces @c$N@W quickly and opens $s mouth wide. A @Yg@yo@Yl@yd@Ye@yn@W glow forms from deep in $s throat, growing brighter as it rises up and out $s mouth. Suddenly a powerful Eraser Cannon erupts and slams into @c$N@W's " + loc + "!@n");   
    }


    // TwinSlash
    bool TwinSlash::checkOtherConditions() {
        if (!GET_EQ(user, WEAR_WIELD1)) {
            send_to_char(user, "You need to wield a sword to use this.\r\n");
            return false;
        }
        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != TYPE_SLASH - TYPE_HIT) {
            send_to_char(user, "You are not wielding a sword, you need one to use this technique.\r\n");
            return false;
        }
        return true;
    }

    void TwinSlash::attackPreprocess() {
        auto wobj = GET_EQ(user, WEAR_WIELD1);
        int wlvl = 0;

        if (OBJ_FLAGGED(wobj, ITEM_WEAPLVL1)) {
            wlvl = 1;
        } else if (OBJ_FLAGGED(wobj, ITEM_WEAPLVL2)) {
            wlvl = 2;
        } else if (OBJ_FLAGGED(wobj, ITEM_WEAPLVL3)) {
            wlvl = 3;
        } else if (OBJ_FLAGGED(wobj, ITEM_WEAPLVL4)) {
            wlvl = 4;
        } else if (OBJ_FLAGGED(wobj, ITEM_WEAPLVL5)) {
            wlvl = 5;
        }

        if (initSkill >= 100) {
            calcDamage += (calcDamage * 0.05) * wlvl;
        } else if (initSkill >= 60) {
            calcDamage += (calcDamage * 0.02) * wlvl;
        } else if (initSkill >= 40) {
            calcDamage += (calcDamage * 0.01) * wlvl;
        }
    }

    void TwinSlash::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                if (victim->hasTail()) {
                    act("@rYou cut off $S tail!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@rYour tail is cut off!@n", true, user, nullptr, victim, TO_VICT);
                    act("@R$N@r's tail is cut off!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->loseTail();
                }
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0) + 2);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 1) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severs $N's left arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your left arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left arm is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 1) = 0;
                        remove_limb(victim, 2);
                    } else if (GET_LIMBCOND(victim, 0) > 0 && !is_sparring(user)) {
                        act("@RYour attack severs $N's right arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your right arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right arm is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 0) = 0;
                        remove_limb(victim, 1);
                    }
                }
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 3) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severs $N's left leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your left leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left leg is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 3) = 0;
                        remove_limb(victim, 4);
                    } else if (GET_LIMBCOND(victim, 2) > 0 && !is_sparring(user)) {
                        act("@RYour attack severs $N's right leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your right leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right leg is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 2) = 0;
                        remove_limb(victim, 3);
                    }
                }
                break;
        }
    }

    void TwinSlash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "guts";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou channel your charged ki into the blade of your sword. @rF@Rl@Ya@rm@Ri@Yn@rg @gg@Gre@wen@W energy burns around the blade as you draw it up to attack. Two blindingly quick slashes connect with @c$N@W's " + loc + " as you fly past, leaving a green after-image behind!@n");
        actVictim("@C$n @Wchannels $s charged ki into the blade of $s sword. @rF@Rl@Ya@rm@Ri@Yn@rg @gg@Gre@wen@W energy burns around the blade as $e draws it up to attack. Two blindingly quick slashes connect with YOUR " + loc + " as $e flies past, leaving a green after-image behind!@n");
        actOthers("@C$n @Wchannels $s charged ki into the blade of $s sword. @rF@Rl@Ya@rm@Ri@Yn@rg @gg@Gre@wen@W energy burns around the blade as $e draws it up to attack. Two blindingly quick slashes connect with @c$N@W's " + loc + " as $e flies past, leaving a green after-image behind!@n");   
    }


    // PsychicBlast
    void PsychicBlast::attackPostprocess() {
        if (GET_CHARGE(victim) > 0 && rand_number(1, 3) == 2) {
            GET_CHARGE(victim) -= calcDamage / 5;
            if (GET_CHARGE(victim) < 0) {
                GET_CHARGE(victim) = 0;
            }
            send_to_char(victim, "@RYou lose some of your charged ki!@n\r\n");
        }

        if (!AFF_FLAGGED(victim, AFF_SHOCKED) && rand_number(1, 4) == 4 && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
            act("@MYour mind has been shocked!@n", true, victim, nullptr, nullptr, TO_CHAR);
            act("@M$n@m's mind has been shocked!@n", true, victim, nullptr, nullptr, TO_ROOM);
            victim->affected_by.set(AFF_SHOCKED);
        }
    }

    void PsychicBlast::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
            case 3:
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou gather your charged ki into your brain as a flash of @bb@Bl@wue@W light shoots from your forehead and slams into @c$N@W's " + loc + "! $E screams for a moment as terrifying images sear through $S mind!@n");
        actVictim("@C$n@W gathers $s charged ki into $s brain as a flash of @bb@Bl@wue@W light shoots from $s forehead and slams into YOUR " + loc + "! You scream for a moment as terrifying images sear through your mind!@n");
        actOthers("@C$n@W gathers $s charged ki into $s brain as a flash of @bb@Bl@wue@W light shoots from $s forehead and slams into @c$N@W's " + loc + "! $E screams for a moment as terrifying images sear through $S mind!@n");   
    }


    //Honoo
    void Honoo::attackPreprocess() {
        if (check_ruby(user) == 1) {
                calcDamage += calcDamage * 0.2;
            }
            if (GET_BONUS(victim, BONUS_FIREPROOF)) {
                calcDamage -= calcDamage * 0.4;
            } else if (GET_BONUS(victim, BONUS_FIREPRONE)) {
                calcDamage += calcDamage * 0.4;
            }

            victim->affected_by.set(AFF_ASHED);
    }

    void Honoo::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_BURNED) && rand_number(1, 4) == 3 && !IS_DEMON(victim) &&
                !GET_BONUS(victim, BONUS_FIREPROOF)) {
                send_to_char(victim, "@RYou are burned by the attack!@n\r\n");
                send_to_char(user, "@RThey are burned by the attack!@n\r\n");
                victim->affected_by.set(AFF_BURNED);
            } else if (GET_BONUS(victim, BONUS_FIREPROOF) || IS_DEMON(victim)) {
                send_to_char(user, "@RThey appear to be fireproof!@n\r\n");
            } else if (GET_BONUS(victim, BONUS_FIREPRONE)) {
                send_to_char(victim, "@RYou are extremely flammable and are burned by the attack!@n\r\n");
                send_to_char(user, "@RThey are easily burned!@n\r\n");
                victim->affected_by.set(AFF_BURNED);
            }
            if (GET_SKILL_PERF(user, SKILL_HONOO) == 3 && attPerc > 0.1) {
                pcost(user, attPerc - 0.05, 0);
            } else {
                pcost(user, attPerc, 0);
            }
            if (ROOM_EFFECT(IN_ROOM(user)) < -1) {
                send_to_room(IN_ROOM(user), "The water surrounding the area evaporates some!\r\n");
                ROOM_EFFECT(IN_ROOM(user)) += 1;
            } else if (ROOM_EFFECT(IN_ROOM(user)) == -1) {
                send_to_room(IN_ROOM(user), "The water surrounding the area evaporates completely away!\r\n");
                ROOM_EFFECT(IN_ROOM(user)) = 0;
            }
            victim->affected_by.reset(AFF_ASHED);
    }

    void Honoo::postProcess() {
        if (ROOM_EFFECT(IN_ROOM(user)) < -1) {
            send_to_room(IN_ROOM(user), "The water surrounding the area evaporates some!\r\n");
            ROOM_EFFECT(IN_ROOM(user)) += 1;
        } else if (ROOM_EFFECT(IN_ROOM(user)) == -1) {
            send_to_room(IN_ROOM(user), "The water surrounding the area evaporates completely away!\r\n");
            ROOM_EFFECT(IN_ROOM(user)) = 0;
        }
    }   

    void Honoo::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
            case 3:
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou gather your charged ki and bring it up into your throat while mixing it with the air in your lungs. You grin evily at @c$N@W before unleashing a massive jet of @rf@Rl@Ya@rm@Re@Ys@W from your lips! @c$N@W's " + loc + " is engulfed!@n");
        actVictim("@C$n@W gathers $s charged ki and brings it up into $s throat while mixing it with the air in $s lungs. $e grins evily at you before unleashing a massive jet of @rf@Rl@Ya@rm@Re@Ys@W from $s lips! YOUR " + loc + " is engulfed!@n");
        actOthers("@C$n@W gathers $s charged ki and brings it up into $s throat while mixing it with the air in $s lungs. $e grins evily at @c$N@W before unleashing a massive jet of @rf@Rl@Ya@rm@Re@Ys@W from $s lips! @c$N@W's " + loc + " is engulfed!@n");   
    }


    //DualBeam
    void DualBeam::processAttack() {
        int mult_count = 3;

        while (mult_count > 0) {
            mult_count -= 1;
            switch(doAttack()) {
                case Result::Landed:
                    onLanded();
                    onLandedOrMissed();
                    break;
                case Result::Missed:
                    onMissed();
                    onLandedOrMissed();
                    break;
                case Result::Canceled:
                    onCanceled();
                    break;
            }
        }
    }

    void DualBeam::handleHitspot() {
        if (initSkill >= 100) {
            if (rand_number(1, 100) >= 60) {
                hitspot = 2;
            }
        } else if (initSkill >= 60) {
            if (rand_number(1, 100) >= 80) {
                hitspot = 2;
            }
        } else if (initSkill >= 40) {
            if (rand_number(1, 100) >= 95) {
                hitspot = 2;
            }
        }

        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0));
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void DualBeam::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
            case 3:
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou gather your charged energy up through the circuits in your arms. A @gg@Gr@We@wen @Wglow appears around your hand right as you aim it at @c$N@W. A @gg@Gr@We@wen @Wbeam blasts out and slams into $S " + loc + " in that instant!@n");
        actVictim("@C$n@W gathers $s charged energy up through the circuits in $s arms. A @gg@Gr@We@wen @Wglow appears around $s hand right as $e aims it at YOU. A @gg@Gr@We@wen @Wbeam blasts out and slams into YOUR " + loc + " in that instant!@n");
        actOthers("@C$n@W gathers $s charged energy up through the circuits in $s arms. A @gg@Gr@We@wen @Wglow appears around $s hand right as $e aims it at @c$N@W. A @gg@Gr@We@wen @Wbeam blasts out and slams into $S " + loc + " in that instant!@n");   
    }


    // Kienzan
    void Kienzan::attackPreprocess() {
        if (initSkill >= 100) {
            calcDamage += calcDamage * 0.25;
        } else if (initSkill >= 60) {
            calcDamage += calcDamage * 0.15;
        } else if (initSkill >= 40) {
            calcDamage += calcDamage * 0.05;
        }
    }

    void Kienzan::handleHitspot() {
        int calcbonus = 0;
        switch(hitspot) {
            case 1:
                act("@R$N's@r body is torn by the attack!@n", true, user, nullptr, victim, TO_CHAR);
                act("@rYour body is torn by the attack!@n", true, user, nullptr, victim, TO_VICT);
                act("@R$N's@r body is torn by the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                calcbonus = 2;
                
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2) + calcbonus;
                else
                    calcDamage *= calcbonus;

                if (victim->hasTail()) {
                    act("@rYou cut off $S tail!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@rYour tail is cut off!@n", true, user, nullptr, victim, TO_VICT);
                    act("@R$N@r's tail is cut off!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->loseTail();
                }
                break;
            case 2:
                act("@R$N's@r face is horrifically torn by the attack!@n", true, user, nullptr, victim, TO_CHAR);
                act("@rYour face is horrifically torn by the attack!@n", true, user, nullptr, victim, TO_VICT);
                act("@R$N's@r face is horrifically torn by the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                calcbonus = 3;

                calcDamage *= (calc_critical(user, 0)) + calcbonus;
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 1) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severes $N's left arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severes your left arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left arm is severed in the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                        GET_LIMBCOND(victim, 1) = 0;
                        remove_limb(victim, 2);
                    } else if (GET_LIMBCOND(victim, 0) > 0 && !is_sparring(user)) {
                        act("@RYour attack severes $N's right arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severes your right arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right arm is severed in the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                        GET_LIMBCOND(victim, 0) = 0;
                        remove_limb(victim, 1);
                    }
                }
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 3) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severes $N's left leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severes your left leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left leg is severed in the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                        GET_LIMBCOND(victim, 3) = 0;
                        remove_limb(victim, 4);
                    } else if (GET_LIMBCOND(victim, 2) > 0 && !is_sparring(user)) {
                        act("@RYour attack severes $N's right leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severes your right leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right leg is severed in the attack!@n", true, user, nullptr, victim, TO_NOTVICT);
                        GET_LIMBCOND(victim, 2) = 0;
                        remove_limb(victim, 3);
                    }
                }
                break;
        }
    }

    void Kienzan::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "neck";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou raise your hand above your head and pool your charged ki above your flattened palm. Slowly a golden spinning disk of energy grows from the ki. With the attack complete you shout '@yK@Yi@we@yn@Yz@wa@yn@W!' and throw it! You watch as it slices into @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W raises $s hand above $s head and pools $s charged ki above $s flattened palm. Slowly a golden spinning disk of energy grows from the ki. With the attack complete $e shouts '@yK@Yi@we@yn@Yz@wa@yn@W!' and throws it! @C$n@W watches as it slices into YOUR " + loc + "!@n");
        actOthers("@C$n@W raises $s hand above $s head and pools $s charged ki above $s flattened palm. Slowly a golden spinning disk of energy grows from the ki. With the attack complete $e shouts '@yK@Yi@we@yn@Yz@wa@yn@W!' and throws it! @C$n@W watches as it slices into @c$N@W's " + loc + "!@n");   
    }


    // Tribeam
    void Tribeam::attackPreprocess() {
        if (initSkill >= 100) {
            calcDamage += (user->getMaxLF()) * 0.20;
        } else if (initSkill >= 60) {
            calcDamage += (user->getMaxLF()) * 0.10;
        } else if (initSkill >= 40) {
            calcDamage += (user->getMaxLF()) * 0.05;
        }
    }

    void Tribeam::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou gather your charged ki as you form a triangle with your hands. You aim the gap between your hands at @c$N@W and shout '@rT@RR@YI@rB@RE@YA@rM@W!'. A bright blast of energy explodes from your hands and slams into $S " + loc + "!@n");
        actVictim("@C$n@W gathers $s charged ki as $e forms a triangle with $s hands. @C$n@W aims the gap between $s hands at YOU and shouts '@rT@RR@YI@rB@RE@YA@rM@W!'. A bright blast of energy explodes from $s hands and slams into YOUR " + loc + "!@n");
        actOthers("@C$n@W gathers $s charged ki as $e forms a triangle with $s hands. @C$n@W aims the gap between $s hands at @c$N@W and shouts '@rT@RR@YI@rB@RE@YA@rM@W!'. A bright blast of energy explodes from $s hands and slams into @c$N@W's " + loc + "!@n");   
    }

    // SpecialBeamCannon
    void SpecialBeamCannon::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou place your index and middle fingers against your forehead and pool your charged ki there. Sparks and light surround your fingertips as the technique becomes ready. You point your fingers at @c$N@W and yell '@YS@yp@De@Yc@yi@Da@Yl @yB@De@Ya@ym @DC@Ya@yn@Dn@Yo@yn@W!' A spiraling beam of energy fires from your fingers and slams into $S " + loc + "!@n");
        actVictim("@C$n@W places $s index and middle fingers against $s forehead and pools $s charged ki there. Sparks and light surround the fingertips as the technique becomes ready. @C$n@W points $s fingers at YOU and yells '@YS@yp@De@Yc@yi@Da@Yl @yB@De@Ya@ym @DC@Ya@yn@Dn@Yo@yn@W!' A spiraling beam of energy fires from the fingers and slams into YOUR " + loc + "!@n");
        actOthers("@C$n@W places $s index and middle fingers against $s forehead and pools $s charged ki there. Sparks and light surround the fingertips as the technique becomes ready. @C$n@W points $s fingers at @c$N@W and yells '@YS@yp@De@Yc@yi@Da@Yl @yB@De@Ya@ym @DC@Ya@yn@Dn@Yo@yn@W!' A spiraling beam of energy fires from the fingers and slams into @c$N@W's " + loc + "!@n");   
    }

    // PsychicBarrage
    void PsychicBarrage::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_MBREAK) && rand_number(1, 4) == 4 && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
            act("@mYour mind's eye has been shattered, you can't charge ki until you recover!@n", true, victim,
                nullptr, nullptr,
                TO_CHAR);
            act("@M$n@m's mind has been damaged by the attack!@n", true, victim, nullptr, nullptr, TO_ROOM);
            victim->affected_by.set(AFF_MBREAK);
        } else if (!AFF_FLAGGED(victim, AFF_SHOCKED) && rand_number(1, 4) == 4 && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
            act("@MYour mind has been shocked!@n", true, victim, nullptr, nullptr, TO_CHAR);
            act("@M$n@m's mind has been shocked!@n", true, victim, nullptr, nullptr, TO_ROOM);
            victim->affected_by.set(AFF_SHOCKED);
        }
    }

    void PsychicBarrage::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou close your eyes for a moment as your charged ki is pooled into your brain. Your eyes snap open, flashing with @bb@Bl@Wu@we @yl@Yi@Wg@wht@W, and look at @c$N@W intensly. Invisible waves of psychic energy slam into $S " + loc + "!@n");
        actVictim("@C$n@W closes $s eyes for a moment as $s charged ki is pooled into $s brain. @C$n@W's eyes snap open, flashing with @bb@Bl@Wu@we @yl@Yi@Wg@wht@W, and looks at YOU intensly. Invisible waves of psychic energy slam into YOUR " + loc + "!@n");
        actOthers("@C$n@W closes $s eyes for a moment as $s charged ki is pooled into $s brain. @C$n@W's eyes snap open, flashing with @bb@Bl@Wu@we @yl@Yi@Wg@wht@W, and looks at @c$N@W intensly. Invisible waves of psychic energy slam into @c$N@W's " + loc + "!@n");   
    }


    // Spiritball
    void Spiritball::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou raise your palm upward with your arm bent in front of you. Your charged ki slowly begins to creep up your arm and form a large glowing orb of energy above your upraised palm. With your @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wcompleted you move your hand with index and middle fingers pointing at @c$N@W! The @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wflies at $M, slamming into $S " + loc + ", and explodes with a load roar!@n");
        actVictim("@C$n@W raises $s palm upward with $s arm bent in front of $m. @C$n@W's charged ki slowly begins to creep up $s arm and form a large glowing orb of energy above $s upraised palm. With $s @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wcompleted $e moves $s hand with index and middle fingers pointing at YOU! The @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wflies at you, slamming into YOUR " + loc + ", and explodes with a load roar!@n");
        actOthers("@C$n@W raises $s palm upward with $s arm bent in front of $m. @C$n@W's charged ki slowly begins to creep up $s arm and form a large glowing orb of energy above $s upraised palm. With $s @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wcompleted $e moves $s hand with index and middle fingers pointing at @c$N@W! The @yS@Yp@Wi@wri@yt @YB@Wa@wll @Wflies at $M, slamming into @c$N@W's " + loc + ", and explodes with a load roar!@n");   
    }


    // Deathball
    void Deathball::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou raise your hand with its index finger extended upwards. Your charged ki begins to pool above that finger, forming a small @rred@W orb of energy. The orb of energy quickly grows to an enormous size as you look down on @c$N@W. You move the hand that formed the attack and point at $M as the @rD@Re@Da@rt@Rh@Db@ra@Rl@Dl@W above you follows the movement! It descends on $M and crushes into $S " + loc + " before exploding into a massive blast!@n");
        actVictim("@C$n@W raises $s hand with its index finger extended upwards. @C$n@W's charged ki begins to pool above that finger, forming a small @rred@W orb of energy. The orb of energy quickly grows to an enormous size as $e looks down on YOU. @C$n@W moves the hand that formed the attack and points at YOU as the @rD@Re@Da@rt@Rh@Db@ra@Rl@Dl@W above $m follows the movement! It descends on YOU and crushes into YOUR " + loc + " before exploding into a massive blast!@n");
        actOthers("@C$n@W raises $s hand with its index finger extended upwards. @C$n@W's charged ki begins to pool above that finger, forming a small @rred@W orb of energy. The orb of energy quickly grows to an enormous size as $e looks down on @c$N@W. @C$n@W moves the hand that formed the attack and points at @c$N@W as the @rD@Re@Da@rt@Rh@Db@ra@Rl@Dl@W above $m follows the movement! It descends on @c$N@W and crushes into $S " + loc + " before exploding into a massive blast!@n");   
    }


    // PhoenixSlash
    bool PhoenixSlash::checkOtherConditions() {
        if (!GET_EQ(user, WEAR_WIELD1)) {
            send_to_char(user, "You need to wield a sword to use this.\r\n");
            return false;
        }
        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != TYPE_SLASH - TYPE_HIT) {
            send_to_char(user, "You are not wielding a sword, you need one to use this technique.\r\n");
            return false;
        }
        return true;
    }

    void PhoenixSlash::attackPreprocess() {
        if (check_ruby(user) == 1) {
            calcDamage += calcDamage * 0.2;
        }
        if (GET_BONUS(victim, BONUS_FIREPROOF)) {
            calcDamage -= calcDamage * 0.4;
        } else if (GET_BONUS(victim, BONUS_FIREPRONE)) {
            calcDamage += calcDamage * 0.4;
        }
        victim->affected_by.set(AFF_ASHED);
    }

    void PhoenixSlash::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_BURNED) && rand_number(1, 4) == 4 && !IS_DEMON(victim) &&
            !GET_BONUS(victim, BONUS_FIREPROOF)) {
            send_to_char(victim, "@RYou are burned by the attack!@n\r\n");
            send_to_char(user, "@RThey are burned by the attack!@n\r\n");
            victim->affected_by.set(AFF_BURNED);
        } else if (GET_BONUS(victim, BONUS_FIREPROOF) || IS_DEMON(victim)) {
            send_to_char(user, "@RThey appear to be fireproof!@n\r\n");
        } else if (GET_BONUS(victim, BONUS_FIREPRONE)) {
            send_to_char(victim, "@RYou are extremely flammable and are burned by the attack!@n\r\n");
            send_to_char(user, "@RThey are easily burned!@n\r\n");
            victim->affected_by.set(AFF_BURNED);
        }
        victim->affected_by.reset(AFF_ASHED);
    }

    int PhoenixSlash::limbhurtChance() {
        return 170;
        
    }

    void PhoenixSlash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou pour your charged ki into your sword's blade. @YF@ri@Re@Yr@ry @Rf@Yl@ra@Rm@Ye@rs@W surround the entire sword in the same instant that you pull the blade back and extend it behind your body. Suddenly you swing the blade forward toward @c$N@W, unleashing a large wave of flames! The flames take the shape of a large phoenix soaring towards @c$N@W! The Phoenix Slash engulfs $S " + loc + " in flames a moment later!@n");
        actVictim("@C$n@W pours $s charged ki into $s sword's blade. @YF@ri@Re@Yr@ry @Rf@Yl@ra@Rm@Ye@rs@W surrounds the entire sword in the same instant that $e pulls the blade back and extends it behind $s body. Suddenly $e swings the blade forward toward YOU, unleashing a large wave of flames! The flames take the shape of a large phoenix soaring towards YOU! The Phoenix Slash engulfs YOUR " + loc + " in flames a moment later!@n");
        actOthers("@C$n@W pours $s charged ki into $s sword's blade. @YF@ri@Re@Yr@ry @Rf@Yl@ra@Rm@Ye@rs@W surrounds the entire sword in the same instant that $e pulls the blade back and extends it behind $s body. Suddenly $e swings the blade forward toward @c$N@W, unleashing a large wave of flames! The flames take the shape of a large phoenix soaring towards @c$N@W! The Phoenix Slash engulfs @c$N@W's " + loc + " in flames a moment later!@n");   
    }


    // BigBang
    void BigBang::handleHitspot() {
        switch(hitspot) {
            case 1:
                calcDamage *= (calc_critical(user, 0));
            case 2:
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void BigBang::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou aim your hand at @c$N@W palm flat up at a ninety degree angle. Your charged ki pools there rapidly before a massive ball of energy explodes from your palm! The @yB@Yi@wg @yB@Ya@wn@yg @Wattack crosses the distance rapidly, slams into @c$N@W's " + loc + ", and explodes leaving behind a mushroom cloud shortly after!@n");
        actVictim("@C$n@W aims $s hand at YOU palm flat up at a ninety degree angle. @C$n@W's charged ki pools there rapidly before a massive ball of energy explodes from $s palm! The @yB@Yi@wg @yB@Ya@wn@yg @Wattack crosses the distance rapidly, slams into YOUR " + loc + ", and explodes leading behind a mushroom cloud shortly after!@n");
        actOthers("@C$n@W aims $s hand at @c$N@W palm flat up at a ninety degree angle. @C$n@W's charged ki pools there rapidly before a massive ball of energy explodes from $s palm! The @yB@Yi@wg @yB@Ya@wn@yg @Wattack crosses the distance rapidly, slams into @c$N@W's " + loc + ", and explodes leading behind a mushroom cloud shortly after!@n");   
    }


    // ScatterShot
    std::optional<int> ScatterShot::hasCooldown() {
        int cool = 8;

        if (IS_PICCOLO(user)) {
            if (initSkill >= 100)
                cool -= 3;
            else if (initSkill >= 75)
                cool -= 2;
            else if (initSkill >= 40)
                cool -= 1;
        }

        if (cool < 1)
            cool = 1;

        return cool;
    }

    void ScatterShot::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou bring your charged ki to your palms and furiously begin to throw hundreds of kiballs at @C$N@W! The kiballs surround $M from every direction in a sphere, preventing $S escape! You hold out your hand and clench it dramatically as your @yS@Yc@ra@Rt@yt@Yer @yS@Yh@ro@Rt@W kiballs close in and explode against @C$N@W's " + loc + "!@n");
        actVictim("@C$n@W brings $s charged ki to $s palms and furiously begins to throw hundreds of kiballs at YOU! The kiballs surround you from every direction in a sphere, preventing your escape! @C$n@W holds out $s hand and clenches it dramatically as $s @yS@Yc@ra@Rt@yt@Yer @yS@Yh@ro@Rt@W kiballs close in and explode against YOUR " + loc + "!@n");
        actOthers("@C$n@W brings $s charged ki to $s palms and furiously begins to throw hundreds of kiballs at @C$N@W! The kiballs surround $M from every direction in a sphere, preventing $S escape! @C$n@W holds out $s hand and clenches it dramatically as $s @yS@Yc@ra@Rt@yt@Yer @yS@Yh@ro@Rt@W kiballs close in and explode against @C$N@W's " + loc + "!@n");   
    }


    // Balefire
    std::optional<int> Balefire::hasCooldown() {
         int cool = 8;

    if (IS_PICCOLO(user)) {
        if (initSkill >= 100)
            cool -= 3;
        else if (initSkill >= 75)
            cool -= 2;
        else if (initSkill >= 40)
            cool -= 1;
    }

    if (cool < 1)
        cool = 1;

        return cool;    
    }

    void Balefire::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou open yourself to the source and begin tracing weaves, the most complex combination, with just the right amounts of each! Your innate talent finishes the final weave and a giant bar of @WB@Ra@Yl@Wef@Yi@Rr@We@W lances out from your hands! Your @WB@Ra@Yl@Wef@Yi@Rr@We@W crosses the distance quickly and slams into @C$N@W's " + loc + "!@n");
        actVictim("@C$n@W begins to glow with a soft, white aura! Their hands begin manipulating something unseen when suddenly a giant bar of @WB@Ra@Yl@Wef@Yi@Rr@We@W lances out from $s hands, crossing the distance and slamming into YOUR " + loc + "!@n");
        actOthers("@C$n@W begins to glow with a soft, white aura! Their hands begin manipulating something unseen when suddenly a giant bar of @WB@Ra@Yl@Wef@Yi@Rr@We@W lances out from $s hands, crossing the distance and slamming into @C$N@W's " + loc + "!@n");   
    }


    // Hellflash
    void Hellflash::attackPreprocess() {
        if (GET_BARRIER(victim) > 0) {
            GET_BARRIER(victim) -= calcDamage;
            if (GET_BARRIER(victim) <= 0) {
                GET_BARRIER(victim) = 1;
            }
        }
    }

    void Hellflash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou stick one of your hands in each of your armpits and by twisting you remove them. Then you aim your exposed wrist cannons at @c$N@W as your charged energy begins to be channeled into the cannons. Shouting '@YH@re@Rl@Yl @rF@Rl@Ya@rs@Rh@W' two large blasts of energy explode from the cannons and slam into @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W sticks one of $s hands in each of $s armpits and by twisting $e removes them. Then $e aims $s exposed wrist cannons at YOU as $s charged energy begins to be channeled into the cannons. @C$n@W shouts '@YH@re@Rl@Yl @rF@Rl@Ya@rs@Rh@W' as two large blasts of energy explode from the cannons and slam into YOUR " + loc + "!@n");
        actOthers("@C$n@W sticks one of $s hands in each of $s armpits and by twisting $e removes them. Then $e aims $s exposed wrist cannons at @c$N@W as $s charged energy begins to be channeled into the cannons. @C$n@W shouts '@YH@re@Rl@Yl @rF@Rl@Ya@rs@Rh@W' as two large blasts of energy explode from the cannons and slam into @c$N@W's " + loc + "!@n");   
    }


    // DarknessDragonSlash
    bool DarknessDragonSlash::checkOtherConditions() {
        if (!GET_EQ(user, WEAR_WIELD1)) {
            send_to_char(user, "You need to wield a sword to use this.\r\n");
            return false;
        }
        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != TYPE_SLASH - TYPE_HIT) {
            send_to_char(user, "You are not wielding a sword, you need one to use this technique.\r\n");
            return false;
        }
        return true;
    }

    void DarknessDragonSlash::attackPreprocess() {
        if (initSkill >= 100) {
            calcDamage += calcDamage * 0.15;
        } else if (initSkill >= 60) {
            calcDamage += calcDamage * 0.10;
        } else if (initSkill >= 40) {
            calcDamage += calcDamage * 0.05;
        }
    }

    void DarknessDragonSlash::attackPostprocess() {
        if (rand_number(1, 3) == 3 && !AFF_FLAGGED(victim, AFF_BLIND)) {
            act("@mYou are struck blind temporarily!@n", true, victim, nullptr, nullptr, TO_CHAR);
            act("@c$n@m is struck blind by the attack!@n", true, victim, nullptr, nullptr, TO_ROOM);
            int duration = 1;
            assign_affect(victim, AFF_BLIND, SKILL_SOLARF, duration, 0, 0, 0, 0, 0, 0);
        }
    }

    int DarknessDragonSlash::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 180;
        }
        return 0;
    }

    void DarknessDragonSlash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou channel your charged ki into the blade of your sword. The energy takes the form of @md@Ma@Wr@wk@W @mf@Ml@Wa@wmes@W burning along the very blade's edge. Swinging your sword at @c$N@W you unleash a serpentine dragon of @md@Ma@Wr@wk @mf@Ml@Wa@wmes@W! The fiery dragon slams into @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W channels $s charged ki into the blade of $s sword. The energy takes the form of @md@Ma@Wr@wk@W @mf@Ml@Wa@wmes@W burning along the very blade's edge. Swinging $s sword at YOU $e unleashes a serpentine dragon of @md@Ma@Wr@wk @mf@Ml@Wa@wmes@W! The fiery dragon slams into YOUR " + loc + "!@n");
        actOthers("@C$n@W channels $s charged ki into the blade of $s sword. The energy takes the form of @md@Ma@Wr@wk@W @mf@Ml@Wa@wmes@W burning along the very blade's edge. Swinging $s sword at @c$N@W $e unleashes a serpentine dragon of @md@Ma@Wr@wk @mf@Ml@Wa@wmes@W! The fiery dragon slams into @c$N@W's " + loc + "!@n");   
    }


    // CrusherBall
    void CrusherBall::attackPostprocess() {
        if (rand_number(1, 3) == 3) {
            tech_handle_crashdown(user, victim);
        }
    }

    void CrusherBall::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou zoom up into the air higher than @c$N@W and raise your hand. Charged @rred@W ki pools above that hand in a large blazing ball. Looking down on @c$N@W you slam your hand into the ball of energy while shouting '@rC@Rr@Wu@wsher @rB@Ra@Wl@wl!@W' Moments later the ball of energy slams into $S " + loc + "!@n");
        actVictim("@C$n@W zooms up into the air higher than you and raises $s hand. Charged @rred@W ki pools above that hand in a large blazing ball. Looking down on YOU $e slams $s hand into the ball of energy while shouting '@rC@Rr@Wu@wsher @rB@Ra@Wl@wl!@W' Moments later the ball of energy slams into YOUR " + loc + "!@n");
        actOthers("@C$n@W zooms up into the air higher than @c$N@W and raises $s hand. Charged @rred@W ki pools above that hand in a large blazing ball. Looking down on @c$N@W $e slams $s hand into the ball of energy while shouting '@rC@Rr@Wu@wsher @rB@Ra@Wl@wl!@W' Moments later the ball of energy slams into @c$N@W's " + loc + "!@n");   
    }


    // FinalFlash
    int FinalFlash::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 180;
        }
        return 0;
    }

    void FinalFlash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou grin as you bring either hand to the sides of your body. Your charged ki begins to pool there as @bblue@W orbs of energy form in your palms. You quickly bring both hands forward slamming your wrists together with the palms flat out facing @c$N@W! You shout '@DF@ci@Cn@Da@cl @CF@Dl@ca@Cs@Dh@W!' as a massive wave of energy erupts all over @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W grins as $e brings either hand to the sides of $s body. @C$n@W's charged ki begins to pool there as @bblue@W orbs of energy form in $s palms. @C$n@W quickly brings both hands forward slamming $s wrists together with the palms flat out facing YOU! @C$n@W shouts '@DF@ci@Cn@Da@cl @CF@Dl@ca@Cs@Dh@W!' as a massive wave of energy erupts all over YOUR " + loc + "!@n");
        actOthers("@C$n@W grins as $e brings either hand to the sides of $s body. @C$n@W's charged ki begins to pool there as @bblue@W orbs of energy form in $s palms. @C$n@W quickly brings both hands forward slamming $s wrists together with the palms flat out facing @c$N@W! @C$n@W shouts '@DF@ci@Cn@Da@cl @CF@Dl@ca@Cs@Dh@W!' as a massive wave of energy erupts all over @c$N@W's " + loc + "!@n");   
    }


    // Kousengan
    int Kousengan::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 190;
        }
        return 0;
    }

    void Kousengan::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_BURNED) && rand_number(1, 4) == 3 && !IS_DEMON(victim)) {
            send_to_char(victim, "@RYou are burned by the attack!@n\r\n");
            send_to_char(user, "@RThey are burned by the attack!@n\r\n");
            victim->affected_by.set(AFF_BURNED);
        }
    }

    void Kousengan::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou look at @c$N@W and grin. Then bright @Mpink@W lasers shoot from your eyes and slam into $S " + loc + "!@n");
        actVictim("@C$n@W looks at YOU and grins. Then bright @Mpink@W lasers shoot from $s eyes and slam into YOUR " + loc + "!@n");
        actOthers("@C$n@W looks at @c$N@W and grins. Then bright @Mpink@W lasers shoot from $s eyes and slam into @c$N@W's " + loc + "!@n");   
    }

    // ZenBlade
    bool ZenBlade::checkOtherConditions() {
        if (!GET_EQ(user, WEAR_WIELD1)) {
            send_to_char(user, "You need to wield a sword to use this.\r\n");
            return false;
        }
        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != TYPE_SLASH - TYPE_HIT) {
            send_to_char(user, "You are not wielding a sword, you need one to use this technique.\r\n");
            return false;
        }
        return true;
    }

    void ZenBlade::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                if (victim->hasTail()) {
                    act("@rYou cut off $S tail!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@rYour tail is cut off!@n", true, user, nullptr, victim, TO_VICT);
                    act("@R$N@r's tail is cut off!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->loseTail();
                }
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0) + 2);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 1) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severs $N's left arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your left arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left arm is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 1) = 0;
                        remove_limb(victim, 2);
                    } else if (GET_LIMBCOND(victim, 0) > 0 && !is_sparring(user)) {
                        act("@RYour attack severs $N's right arm!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your right arm!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right arm is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 0) = 0;
                        remove_limb(victim, 1);
                    }
                }
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                if (rand_number(1, 100) >= 70 && !IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SANCTUARY)) {
                    if (GET_LIMBCOND(victim, 3) > 0 && !is_sparring(user) && rand_number(1, 1) == 2) {
                        act("@RYour attack severs $N's left leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your left leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's left leg is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 3) = 0;
                        remove_limb(victim, 4);
                    } else if (GET_LIMBCOND(victim, 2) > 0 && !is_sparring(user)) {
                        act("@RYour attack severs $N's right leg!@n", true, user, nullptr, victim, TO_CHAR);
                        act("@R$n's attack severs your right leg!@n", true, user, nullptr, victim, TO_VICT);
                        act("@R$N's right leg is severered in the attack!@n", true, user, nullptr, victim, TO_VICT);
                        GET_LIMBCOND(victim, 2) = 0;
                        remove_limb(victim, 3);
                    }
                }
                break;
        }
    }

    void ZenBlade::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "guts";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@CRaising your blade above your head, and closing your eyes, you focus ki into its edge. The edge of the blade begins to glow a soft blue as the blade begins to throb with excess energy. Peels of lighting begin to arc from the blade in all directions as you open your eyes and instantly move past @g$N's@C body while slashing with the pure energy of your resolve! A large explosion of energy erupts across $S " + loc + "!@n");
        actVictim("@G$n @Craises $s blade above $s head, and closes $s eyes. The edge of the blade begins to glow a soft blue as the blade begins to throb with excess energy. Peels of lighting begin to arc from the blade in all directions as $e opens $s eyes and instantly moves past YOUR body while slashing with the pure energy of $s resolve! A large explosion of energy erupts across YOUR " + loc + "!@n");
        actOthers("@G$n @Craises $s blade above $s head, and closes $s eyes. The edge of the blade begins to glow a soft blue as the blade begins to throb with excess energy. Peels of lighting begin to arc from the blade in all directions as $e opens $s eyes and instantly moves past @g$N's@C body while slashing with the pure energy of $s resolve! A large explosion of energy erupts across @g$N's@C " + loc + "!@n");   
    }


    // MaliceBreaker
    void MaliceBreaker::attackPreprocess() {
        if (time_info.hours <= 15) {
            calcDamage *= 1.25;
        } else if (time_info.hours <= 22) {
            calcDamage *= 1.4;
        }
    }

    void MaliceBreaker::handleHitspot() {
        if (hitspot == 1) hitspot = 2;
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0));
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void MaliceBreaker::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou rush forward at @c$N@W, building ki into your arm. As you slam an open palm into $S chest, you send the charged energy into $S body. A few small explosions seem to hit across $S entire body, forcing $M to stumble back. Finally, you launch $M into the air, pointing a forefinger at $m like a pistol, and shout '@MM@ma@Dl@wi@Wce Br@we@Dak@me@Mr@W!' as a dark, violet explosion erupts at the epicenter of your first strike on @c$N@W's " + loc + "!@n");
        actVictim("@C$n @Wrushes forward at you, building ki into $s arm. As $e slams an open palm into your chest, $e sends the charged energy into your body. A few small explosions seem to hit across your entire body, forcing you to stumble back. Finally, @C$n@W launches you into the air, pointing a forefinger at your body like a pistol, and shouts '@MM@ma@Dl@wi@Wce Br@we@Dak@me@Mr@W!' as a dark, violet explosion erupts at the epicenter of $s first strike on your " + loc + "!@n");
        actOthers("@C$n@W rushes forward at $N@W, building ki into $s arm. As $e slam an open palm into @c$N's@W chest, $e sends the charged energy into $S body. A few small explosions seem to hit across @c$N's@W entire body, forcing $M to stumble back. Finally, @C$n@W launches $M into the air, pointing a forefinger at $m like a pistol, and shouts '@MM@ma@Dl@wi@Wce Br@we@Dak@me@Mr@W!' as a dark, violet explosion erupts at the epicenter of $s first strike on @c$N@W's " + loc + "!@n");   
    }


    // SeishouEnko
    void SeishouEnko::attackPreprocess() {
        if (GET_MOLT_LEVEL(user) >= 150) {
            calcDamage *= 2;
        }
    }

    void SeishouEnko::handleHitspot() {
        if (rand_number(1,3) == 3) hitspot = 2;
        if (hitspot == 1) hitspot = 2;
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0));
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void SeishouEnko::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou aim your mouth at @C$N@W and focus your charged ki. In an instant you fire a large @Rr@re@Rd@W @rS@Re@Wi@ws@rh@Ro@Wu @wE@rn@Rk@Wo at $M! Almost instantly it blasts into $S " + loc + " with searing heat!@n");
        actVictim("@C$n @Waims $s mouth at YOU and seems to focus $s ki. In an instant $e fires a large @Rr@re@Rd@W @rS@Re@Wi@ws@rh@Ro@Wu @wE@rn@Rk@Wo at YOU! Almost instantly it blasts into YOUR " + loc + " with searing heat!@n");
        actOthers("@C$n @Waims $s mouth at @c$N@W and seems to focus $s ki. In an instant $e fires a large @Rr@re@Rd@W @rS@Re@Wi@ws@rh@Ro@Wu @wE@rn@Rk@Wo at $M! Almost instantly it blasts into $S " + loc + " with searing heat!@n");   
    }


    // WaterSpike
    bool WaterRazor::checkOtherConditions() {
        if (IS_ANDROID(victim)) {
            send_to_char(user, "There is not a necessary amount of water in cybernetic creatures.\r\n");
            return false;
        }
        return true;
    }

    void WaterRazor::attackPreprocess() {
        reduction = GET_HIT(victim);
        if (AFF_FLAGGED(user, AFF_SANCTUARY)) {
            if (initSkill >= 100) {
                GET_BARRIER(user) += calcDamage * 0.1;
            } else if (initSkill >= 60) {
                GET_BARRIER(user) += calcDamage * 0.05;
            } else if (initSkill >= 40) {
                GET_BARRIER(user) += calcDamage * 0.02;
            }
            if (GET_BARRIER(user) > GET_MAX_MANA(user)) {
                GET_BARRIER(user) = GET_MAX_MANA(user);
            }
        }
    }

    void WaterRazor::attackPostprocess() {
        if (victim) {
            reduction = reduction - GET_HIT(victim);

            if ((!IS_NPC(victim) && !AFF_FLAGGED(victim, AFF_SPIRIT)) || (IS_NPC(victim) && GET_HIT(victim) > 0)) {
                victim->decCurKI(reduction);
                victim->decCurST(reduction);
            }
        }
    }

    void WaterRazor::handleHitspot() {
        if (rand_number(1,3) == 3) hitspot = 2;
        if (hitspot == 1) hitspot = 2;
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0));
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void WaterRazor::announceHitspot() {
        switch(hitspot) {
            case 2:
            case 3:
                actUser("@WYou raise your hand toward @c$N@W with the palm open as if you are ready to grab the air between the two of you. You then focus your ki into $S body and close your hand in an instant! The @Bwater@W in $S body instantly takes the shape of millions of microscopic @Dblades@W that cut up $S insides! Blood sprays out in a mist from every pore of $S body!@n");
                actVictim("@C$n@W raises $s hand toward YOU with the palm open as if $e is ready to grab the air between the two of you. $e then focus $s ki into YOUR body and close $s hand in an instant! The @Bwater@W in your body instantly takes the shape of millions of microscopic @Dblades@W that cut up YOUR insides! Blood sprays out of your pores into a fine mist!@n");
                actOthers("@C$n@W raises $s hand toward @c$N@W with the palm open as if $e is ready to grab the air between the two of them. $e then focus $s ki into @c$N's@W body and close $s hand in an instant! The @Bwater@W in @c$N's@W body instantly takes the shape of millions of microscopic @Dblades@W that cut up $S insides! Blood sprays out of $S pores into a fine mist!@n");  
                break;
            case 1:
            case 4:
            case 5:
                actUser("@WYou raise your hand toward @c$N@W with the palm open as if you are ready to grab the air between the two of you. You then focus your ki into $S body and close your hand in an instant! The @Bwater@W in $S body instantly takes the shape of millions of microscopic @Dblades@W that cut up $S insides!@n");
                actVictim("@C$n@W raises $s hand toward YOU with the palm open as if $e is ready to grab the air between the two of you. $e then focus $s ki into YOUR body and close $s hand in an instant! The @Bwater@W in your body instantly takes the shape of millions of microscopic @Dblades@W that cut up YOUR insides!@n");
                actOthers("@C$n@W raises $s hand toward @c$N@W with the palm open as if $e is ready to grab the air between the two of them. $e then focus $s ki into @c$N's@W body and close $s hand in an instant! The @Bwater@W in @c$N's@W body instantly takes the shape of millions of microscopic @Dblades@W that cut up $S insides!@n");  
        }
    }


    // WaterSpike
    void WaterSpike::attackPreprocess() {
        if (AFF_FLAGGED(user, AFF_SANCTUARY)) {
            if (initSkill >= 100) {
                GET_BARRIER(user) += calcDamage * 0.1;
            } else if (initSkill >= 60) {
                GET_BARRIER(user) += calcDamage * 0.05;
            } else if (initSkill >= 40) {
                GET_BARRIER(user) += calcDamage * 0.02;
            }
            if (GET_BARRIER(user) > GET_MAX_MANA(user)) {
                GET_BARRIER(user) = GET_MAX_MANA(user);
            }
        }
    }

    void WaterSpike::postProcess() {
        if (initSkill >= 100) {
            user->incCurKI((user->getMaxKI() * attPerc) * .3);
        } else if (initSkill >= 60) {
            user->incCurKI((user->getMaxKI() * attPerc) * .1);
        } else if (initSkill >= 40) {
            user->incCurKI((user->getMaxKI() * attPerc) * .05);
        }
    }

    void WaterSpike::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= (calc_critical(user, 0));
                if (!AFF_FLAGGED(victim, AFF_KNOCKED) &&
                    (rand_number(1, 3) >= 2 && (GET_HIT(victim) > GET_HIT(user) / 5) &&
                        !AFF_FLAGGED(victim, AFF_SANCTUARY))) {
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_CHAR);
                    act("@WYou are knocked out!@n", true, user, nullptr, victim, TO_VICT);
                    act("@C$N@W is knocked out!@n", true, user, nullptr, victim, TO_NOTVICT);
                    victim->setStatusKnockedOut();
                }
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
                calcDamage *= calc_critical(user, 1);
                break;
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void WaterSpike::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@CYou slam both your hands together in front of you, palms flat. As you pull them apart ki flows from your palms and forms into a giant ball of water. You raise your hand above your head with the ball of water. You command the water to form several sharp spikes which freeze upon forming. The spikes then launch at @R$N@C and slam into $S " + loc + "!@n");
        actVictim("@c$n@C slams both $s hands together in front of $mself, palms flat. As $e pulls them apart ki flows from the palms and forms into a giant ball of water. Then $e raises $s hand above $s head with the ball of water. The water forms several sharp spikes which freeze instantly as they take shape. The spikes then launch at @RYOU@C and slam into YOUR " + loc + "!@n");
        actOthers("@c$n@C slams both $s hands together in front of $mself, palms flat. As $e pulls them apart ki flows from the palms and forms into a giant ball of water. Then $e raises $s hand above $s head with the ball of water. The water forms several sharp spikes which freeze instantly as they take shape. The spikes then launch at @R$N@C and slam into $s " + loc + "!@n");   
    }


    // KoteiruBakuha
    void KoteiruBakuha::attackPreprocess() {
        if (AFF_FLAGGED(user, AFF_SANCTUARY)) {
            if (initSkill >= 100) {
                GET_BARRIER(user) += calcDamage * 0.1;
            } else if (initSkill >= 60) {
                GET_BARRIER(user) += calcDamage * 0.05;
            } else if (initSkill >= 40) {
                GET_BARRIER(user) += calcDamage * 0.02;
            }
            if (GET_BARRIER(user) > GET_MAX_MANA(user)) {
                GET_BARRIER(user) = GET_MAX_MANA(user);
            }
        }
    }

    void KoteiruBakuha::attackPostprocess() {
        if (victim) {
            if (GET_HIT(victim) > 1) {
                if (rand_number(1, 4) == 1 && !AFF_FLAGGED(victim, AFF_FROZEN) && !IS_DEMON(victim)) {
                    act("@CYour body completely freezes!@n", true, victim, nullptr, nullptr, TO_CHAR);
                    act("@c$n's@C body completely freezes!@n", true, victim, nullptr, nullptr, TO_ROOM);
                    victim->affected_by.set(AFF_FROZEN);
                }
            }
        }
    }

    int KoteiruBakuha::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 190;
        }
        return 0;
    }

    void KoteiruBakuha::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "body";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@CYou hold your hands outstretched in front of your body and with your ki begin to create turbulant waters that hover around your body. You begin a sort of dance as you control the water more and more until you have created a huge floating wave. In an instant you swing the wave at @W$N@C! As the wave slams into $S " + loc + " it freezes solid around $M!@n");
        actVictim("@c$n@C holds $s hands outstretched in front of $s body and with $s ki begins to create turbulant waters that hover around $s body. @c$n@C begins a sort of dance as $e controls the water more and more until $e has created a huge floating wave. In an instant $e swings the wave at YOU! As the wave slams into YOUR " + loc + " it freezes solid around YOU!@n");
        actOthers("@c$n@C holds $s hands outstretched in front of $s body and with $s ki begins to create turbulant waters that hover around $s body. @c$n@C begins a sort of dance as $e controls the water more and more until $e has created a huge floating wave. In an instant $e swings the wave at @W$N@C! As the wave slams into $S " + loc + " it freezes solid around @W$N@C!@n");   
    }


    // HellSpiral
    int HellSpiral::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 120;
        }
        return 0;
    }

    void HellSpiral::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou hold out your hand, palm upward, while looking toward @C$N@W and grinning as @Rred@W energy begins to pool in the center of your palm. As the large orb of energy swells to about three feet in diameter you move your hand away! You quickly punch the ball of energy and send it flying into @C$N's@W " + loc + " where it @re@Rx@Dp@rl@Ro@Dd@re@Rs@W in a flash of light!@n");
        actVictim("@C$n @Wholds out $s hand, palm upward, while looking toward YOU and grinning as @Rred@W energy begins to pool in the center of $s palm. As the large orb of energy swells to about three feet in diameter $e moves $s hand away! Then $e quickly punches the ball of energy and sends it flying into YOUR " + loc + " where it @re@Rx@Dp@rl@Ro@Dd@re@Rs@W in a flash of light!@n");
        actOthers("@C$n @Wholds out $s hand, palm upward, while looking toward @c$N@W and grinning as @Rred@W energy begins to pool in the center of $s palm. As the large orb of energy swells to about three feet in diameter $e moves $s hand away! Then $e quickly punches the ball of energy and sends it flying into @c$N's@W " + loc + " where it @re@Rx@Dp@rl@Ro@Dd@re@Rs@W in a flash of light!@n");   
    }


    // StarBreaker
    void StarBreaker::attackPreprocess() {
        theft = 0;
        taken = 0;
        auto userInt = GET_INT(user);
        auto vInt = GET_INT(victim);
        auto vExp = GET_EXP(victim);
        if (userInt - 30 > userInt) {
            theft = 1;
        } else if (userInt - 20 > vInt) {
            theft = vExp / 1000;
        } else if (userInt - 10 > vInt) {
            theft = vExp / 100;
        } else if (userInt >= vInt) {
            theft = vExp / 50;
        } else if (userInt + 10 >= vInt) {
            theft = vExp / 500;
        } else if (userInt + 20 >= vInt) {
            theft = vExp / 1000;
        } else {
            theft = vExp / 2000;
        }
        if(theft > 0) {
            taken = victim->modExperience(-theft);
        }
    }

    void StarBreaker::attackPostprocess() {
        if (level_exp(user, GET_INT(user) + 1) - (GET_EXP(user)) > 0 || GET_INT(user) >= 100) {
            auto xp = user->modExperience(theft * 2, false); // we will not apply bonuses to stolen exp.
            send_to_char(user, "The returning Eldritch energy blesses you with some experience. @D[@G%s@D]@n\r\n",
                            add_commas(xp).c_str());

        }
    }

    int StarBreaker::limbhurtChance() {
        switch(hitspot) {
            case 4:
            case 5:
                return 180;
        }
        return 0;
    }

    void StarBreaker::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou raise your right hand above your head with it slightly cupped. Dark @rred@W energy from the Eldritch Star begins to pool there and form a growing orb of energy. At the same time @mpurple@W arcs of electricity formed by your ki flows up your left arm as you raise it. You slam both of your hands together, combining the energy, and then toss your @YS@yta@Yr @rB@Rr@De@ra@Rk@De@rr@W at @c$N@W! It engulfs $S " + loc + "!@n");
        actVictim("@C$n@W raises $s right hand above $s head with it slightly cupped. Dark @rred@W energy begins to pool there and form a growing orb of energy. At the same time @mpurple@W arcs of electricity formed by $s ki flows up $s left arm as $e raises it. Suddenly $e slams both of $s hands together, combining the energy, and then toss a @YS@yta@Yr @rB@Rr@De@ra@Rk@De@rr@W at YOU! It engulfs your " + loc + "!@n");
        actOthers("@C$n@W raises $s right hand above $s head with it slightly cupped. Dark @rred@W energy begins to pool there and form a growing orb of energy. At the same time @mpurple@W arcs of electricity formed by $s ki flows up $s left arm as $e raises it. Suddenly $e slams both of $s hands together, combining the energy, and then tosses a @YS@yta@Yr @rB@Rr@De@ra@Rk@De@rr@W at @c$N@W! It engulfs $S " + loc + "!@n");   
    }


    // FireBreath
    void FireBreath::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_BURNED) && rand_number(1, 4) == 3 && !IS_DEMON(victim) &&
                !GET_BONUS(victim, BONUS_FIREPROOF)) {
            send_to_char(victim, "@RYou are burned by the attack!@n\r\n");
            send_to_char(user, "@RThey are burned by the attack!@n\r\n");
            victim->affected_by.set(AFF_BURNED);
        } else if (GET_BONUS(victim, BONUS_FIREPROOF) || IS_DEMON(victim)) {
            send_to_char(user, "@RThey appear to be fireproof!@n\r\n");
        } else if (GET_BONUS(victim, BONUS_FIREPRONE)) {
            send_to_char(victim, "@RYou are extremely flammable and are burned by the attack!@n\r\n");
            send_to_char(user, "@RThey are easily burned!@n\r\n");
            victim->affected_by.set(AFF_BURNED);
        }
    } 

    void FireBreath::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@CYou@W aim your mouth at @c$N@W and opens it wide slowly. A high pitched sound can be heard buzzes in your mouth, the burning warmth felt easily against your tongue. Suddenly you breathe a jet of @rf@Ri@Ye@Rr@ry@W flames onto @c$n's@W " + loc + "!@n");
        actVictim("@C$n@W aims $s mouth at you and opens it wide slowly. A high pitched sound can be heard as the mouth opens, and as the throat is exposed a bright white flame can be seen burning there. Suddenly @C$n@W breathes a jet of @rf@Ri@Ye@Rr@ry@W flames onto YOUR " + loc + "!@n");
        actOthers("@C$n@W aims $s mouth at @c$N@W and opens it wide slowly. A high pitched sound can be heard as the mouth opens, and as the throat is exposed a bright white flame can be seen burning there. Suddenly @C$n@W breathes a jet of @rf@Ri@Ye@Rr@ry@W flames onto @c$N's@W " + loc + "!@n");   
    }


    // Ram
    void Ram::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@cYou@W aim your head at @C$N@W and ram into $S's " + loc + "!@n");
        actVictim("@C$n@W aims $s head at YOU and rams into YOUR " + loc + "!@n");
        actOthers("@c$n@W aims $s head at @C$N@W and rams into $S " + loc + "!@n");   
    }

    // FangStrike
    void FangStrike::attackPreprocess() {
        calcDamage += calcDamage * 0.5;
    } 

    void FangStrike::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@cYou@W launch your body at @C$N@W and sink your fang strike into $S's " + loc + "!@n");
        actVictim("@C$n@W launches $s body at YOU and sinks $s fang strike into YOUR " + loc + "!@n");
        actOthers("@c$n@W launches $s body at @C$N@W and sinks $s fang strike into $S " + loc + "!@n");   
    }


    // SunderingForce
    int SunderingForce::limbhurtChance() {
        int chance = rand_number(2, 12);

        if (chance >= 10) 
            return 160;
        else if (chance >= 8) {
            targetLimb = 1;
            return 160;
        }
 
        return 0;
    }

    void SunderingForce::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou raise both hands and aim the flats of your palms toward @c$N@W. As you concentrate your charged ki long arcing beams of blue energy shoot out and form a field around $M. With a quick motion you move your hands in opposite directions and wrench @c$N's@W " + loc + " with the force of your energy!@n");
        actVictim("@C$n@W raises both hands and aims the flats of $s palms toward YOU. As $e concentrates $s charged ki long arcing beams of blue energy shoot out and form a field around YOU. With a quick motion $e moves $s hands in opposite directions and wrenches YOUR " + loc + " with the force of $s energy!@n");
        actOthers("@C$n@W raises both hands and aims the flats of $s palms toward @c$N@W. As $e concentrates $s charged ki long arcing beams of blue energy shoot out and form a field around @c$N@W. With a quick motion @C$n@W moves $s hands in opposite directions and wrenches @c$N's@W " + loc + " with the force of $s energy!@n");   
    }


    // Bash
    void Bash::attackPostprocess() {
        if (victim && rand_number(1, 5) >= 4)
            tech_handle_crashdown(victim, user);

        if (rand_number(1, 5) >= 5)
            tech_handle_crashdown(user, victim);
    }

    void Bash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WBending over slightly you aim your body at @c$N@W and instantly launch yourself toward $M at full speed! You slam into $S " + loc + " with a crashing impact!@n");
        actVictim("@C$n@W bends over slightly aiming $s body at YOU and then instantly launches $mself toward YOU at full speed! @C$n@W slams into YOUR " + loc + " with a crashing impact!@n");
        actOthers("@C$n@W bends over slightly aiming $s body at @c$N@W and then instantly launches $mself toward $M at full speed! @C$n@W slams into $S " + loc + " with a crashing impact!@n");   
    }


    // Bite
    void Bite::attackPreprocess() {
        calcDamage += calcDamage * 0.25;
    }

    void Bite::attackPostprocess() {
        if (!IS_NPC(user) && IS_MUTANT(user)) {
            if (axion_dice(0) > GET_CON(victim) && rand_number(1, 5) == 5) {
                act("@R$N@r was poisoned by your bite!@n", true, user, nullptr, victim, TO_CHAR);
                act("@rYou were poisoned by the bite!@n", true, user, nullptr, victim, TO_VICT);
                victim->poisonby = user;
                user->poisoned.insert(victim);
                int duration = (GET_INT(user) / 50) + 1;
                assign_affect(victim, AFF_POISON, SKILL_POISON, duration, 0, 0, 0, 0, 0, 0);
            }
        }
        if((user->form == FormID::Lycanthrope && victim->getCurHealthPercent() <= 0.2) || user->form == FormID::AlphaLycanthrope) {
            if(!victim->transforms.contains(FormID::Lycanthrope) && axion_dice(0) <= 30) {
                victim->addTransform(FormID::Lycanthrope);
                send_to_char(victim, "@rYour heart races, you feel like something is about to tear free of you.@n\n");
            }
        }
    }

    void Bite::announceHitspot() {
        switch (hitspot) {
            case 1:
                actUser("@WYou bite @C$N's@W face!@n");
                actVictim("@C$n@W bites your face!@n");
                actOthers("@c$n@W bites into $N's face!@n");
                break;
            case 2: /* Critical */
                actUser("@WYou bite @C$N@W!@n");
                actVictim("@C$n@W bites you!@n");
                actOthers("@C$n@W bites @c$N@W!@n");
                break;
            case 3:
                actUser("@WYou bite @C$N's@W body!@n");
                actVictim("@C$n@W bites you on the body, sending blood flying!@n");
                actOthers("@C$n@W bites @c$N@W on the body, sending blood flying!@n");
                break;
            case 4: /* Weak */
                actUser("@WYou bite @C$N's@W arm!@n");
                actVictim("@C$n@W bites you on the arm!@n");
                actOthers("@C$n@W bites @c$N@W on the arm!@n");
                break;
            case 5: /* Weak */
                actUser("@WYou bite @C$N's@W leg!@n");
                actVictim("@C$n@W bites into your leg!@n");
                actOthers("@c$n@W bites into @c$N's@W leg!@n");
                break;
        }
    }


    // Rogafufuken
    int Rogafufuken::limbhurtChance() {
        switch(hitspot){
            case 4:
            case 5:
                return 170;
        }
 
        return 0;
    }

    void Rogafufuken::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou pour your charged energy into your hands and feet. A @rr@Re@wd@W glow trails behind the movements of either as you leap towards @c$N@W while yelling '@cW@Co@Wl@wf @DFang @rFist@W!'. You unleash a flurry of hand strikes on @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W pours $s charged energy into $s hands and feet. A @rr@Re@wd@W glow trails behind the movements of either as $e leaps towards YOU while yelling '@cW@Co@Wl@wf @DFang @rFist@W!'. $e unleashes a flurry of hand strikes on YOUR " + loc + "!@n");
        actOthers("@C$n@W pours $s charged energy into $s hands and feet. A @rr@Re@wd@W glow trails behind the movements of either as $e leaps towards @c$N@W while yelling '@cW@Co@Wl@wf @DFang @rFist@W!'. $e unleashes a flurry of hand strikes on @c$N@W's " + loc + "!@n");   
    }


    // Tailwhip
    bool Tailwhip::checkOtherConditions() {
        if (!PLR_FLAGGED(user, PLR_TAIL) && !IS_NPC(user)) {
            send_to_char(user, "You have no tail!\r\n");
            return false;
        }
        return true;
    }

    int Tailwhip::limbhurtChance() {
        switch(hitspot){
            case 4:
            case 5:
                return 195;
        }
 
        return 0;
    }

    void Tailwhip::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou swing your tail and manage to slam it into @c$N@W's " + loc + "!@n");
        actVictim("@C$n@W swings $s tail and manages to slam it into YOUR " + loc + "!@n");
        actOthers("@C$n@W swings $s tail and manages to slam it into @c$N@W's " + loc + "!@n");   
    }


    // KiAreaAttack
    // Gather Targets, check list > 0 after not including some
    // Announce the attack
    // Reduce damage by number of targets?
    // Do attack

    bool KiAreaAttack::getOpponent() {

        std::list<struct char_data*> people = user->getRoom()->getPeople();

        for (auto person : people) {
            if (person == user) {
                continue;
            }
            if (AFF_FLAGGED(person, AFF_SPIRIT) && !IS_NPC(person)) {
                continue;
            }
            if (AFF_FLAGGED(person, AFF_GROUP) && (person->master == user || user->master == person)) {
                continue;
            }
            if (GET_MAX_HIT(person) <= 10000 && !IS_NPC(person)) {
                continue;
            }
            if (MOB_FLAGGED(person, MOB_NOKILL)) {
                continue;
            } else {
                targets.push_back(person);
            }
        }
        if (targets.empty()) {
            send_to_char(user, "There is no one worth targeting around.\r\n");
            return false;
        }
        return true;
    }

    void KiAreaAttack::doUserCost() {
        if(!paidCost) {
            pcost(user, currentKiCost, currentStaminaCost);
            paidCost = true;
        }
    }


    void KiAreaAttack::processAttack() {
        announceAttack();

        for(char_data *value : targets) {
            victim = value;
            switch(doAttack()) {
                case Result::Landed:
                    onLanded();
                    onLandedOrMissed();
                    break;
                case Result::Missed:
                    onMissed();
                    onLandedOrMissed();
                    break;
                case Result::Canceled:
                    onCanceled();
                    break;
            }
        }
        
    }

    void KiAreaAttack::announceAttack() {
        actUser("@WYou raise your hand with index and middle fingers extended upwards. You try releasing your charged ki in a @yB@Ya@Wk@wuh@ya@Yt@Ws@wuh@ya@W but mess up and waste the ki!@n");
        actRoom("@C$n@W raises $s hand with index and middle fingers extended upwards. @C$n@W tries releasing $s charged ki in a @yB@Ya@Wk@wuh@ya@Yt@Ws@wuh@ya@W but messes up and wastes the ki!@n");
    }


    // Bakuhatsuha
    void Bakuhatsuha::attackPreprocess() {
        if(targets.size() >= 4) {
            calcDamage = (calcDamage / 100) * 25;
        } else if(targets.size() == 3) {
            calcDamage = (calcDamage / 100) * 50;
        } else if(targets.size() == 2) {
            calcDamage = (calcDamage / 100) * 75;
        } else if(targets.size() == 1) {
            calcDamage = (calcDamage / 100);
        }
    }

    void Bakuhatsuha::announceAttack() {
        actUser("@WYou raise your hand with index and middle fingers extended upwards. You try releasing your charged ki in a @yB@Ya@Wk@wuh@ya@Yt@Ws@wuh@ya@W but mess up and waste the ki!@n");
        actRoom("@C$n@W raises $s hand with index and middle fingers extended upwards. @C$n@W tries releasing $s charged ki in a @yB@Ya@Wk@wuh@ya@Yt@Ws@wuh@ya@W but messes up and wastes the ki!@n");
    }

    void Bakuhatsuha::announceHitspot() {
        actUser("@R$N@r is caught by the explosion!@n");
        actVictim("@RYou are caught by the explosion!@n");
        actOthers("@R$N@r is caught by the explosion!@n");   
    }

    // Kakusanha
    void Kakusanha::attackPreprocess() {
        if(targets.size() >= 3) {
            calcDamage = (calcDamage / 100) * 40;
        } else if(targets.size() >= 1) {
            calcDamage = (calcDamage / 100) * 60;
        }
    }

    void Kakusanha::announceAttack() {
        actUser("@WYou pour your charged ki into your hands and bring them both forward quickly. @yG@Yo@Wl@wden@W orbs of energy form at the extent of your palms. You fire one massive beam of @yg@Yo@Wl@wden @Wenergy from combining both orbs. The beam flies forward a short distance before you swing your arms upward and the beam follows suit. Above your targets the beam breaks apart into five seperate pieces that follow their victims!@n");
        actRoom("@C$n@W pours $s charged ki into $s hands and brings them both forward quickly. @yG@Yo@Wl@wden@W orbs of energy form at the extent of $s palms. @C$n@W fires one massive beam of @yg@Yo@Wl@wden @Wenergy from combining both orbs. The beam flies forward a short distance before $e swings $s arms upward and the beam follows suit. Above you the beam breaks apart into five seperate pieces that follow their victims!@n");
    }

    void Kakusanha::announceHitspot() {
        actUser("@R$N@r is slammed by one of the beams!@n");
        actVictim("@RYou are slammed by one of the beams!@n");
        actOthers("@R$N@r is slammed by one of the beams!@n");   
    }

    void Kakusanha::postProcess() {
        int count = targets.size();
        if (count < 5 && !ROOM_FLAGGED(IN_ROOM(user), ROOM_SPACE)) {
            send_to_room(IN_ROOM(user), "The rest of the beams slam into the ground!@n\r\n");
            send_to_room(IN_ROOM(user), "@wBright explosions erupt from the impacts!\r\n");

            if (SECT(IN_ROOM(user)) != SECT_INSIDE) {
                impact_sound(user, "@wA loud roar is heard nearby!@n\r\n");
                switch (rand_number(1, 8)) {
                    case 1:
                        act("Debris is thrown into the air and showers down thunderously!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("Debris is thrown into the air and showers down thunderously!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 2:
                        if (rand_number(1, 4) == 4 && ROOM_EFFECT(IN_ROOM(user)) == 0) {
                            ROOM_EFFECT(IN_ROOM(user)) = 5;
                            act("Lava spews up through cracks in the ground, roaring into the sky as a large column of molten rock!",
                                true, user, nullptr, nullptr, TO_CHAR);
                            act("Lava spews up through cracks in the ground, roaring into the sky as a large column of molten rock!",
                                true, user, nullptr, nullptr, TO_ROOM);
                        }
                        break;
                    case 3:
                        act("A cloud of dust envelopes the entire area!", true, user, nullptr, nullptr, TO_CHAR);
                        act("A cloud of dust envelopes the entire area!", true, user, nullptr, nullptr, TO_ROOM);
                        break;
                    case 4:
                        act("The surrounding area roars and shudders from the impact!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The surrounding area roars and shudders from the impact!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 5:
                        act("The ground shatters apart from the stress of the impact!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The ground shatters apart from the stress of the impact!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 6:
                        act("The explosion continues to burn spreading out and devouring some more of the ground before dying out.",
                            true, user, nullptr, nullptr, TO_CHAR);
                        act("The explosion continues to burn spreading out and devouring some more of the ground before dying out.",
                            true, user, nullptr, nullptr, TO_ROOM);
                        break;
                    default:
                        /* we want no message for the default */
                        break;
                }
            }
            if (SECT(IN_ROOM(user)) == SECT_UNDERWATER) {
                switch (rand_number(1, 3)) {
                    case 1:
                        act("The water churns violently!", true, user, nullptr, nullptr, TO_CHAR);
                        act("The water churns violently!", true, user, nullptr, nullptr, TO_ROOM);
                        break;
                    case 2:
                        act("Large bubbles rise from the movement!", true, user, nullptr, nullptr, TO_CHAR);
                        act("Large bubbles rise from the movement!", true, user, nullptr, nullptr, TO_ROOM);
                        break;
                    case 3:
                        act("The water collapses in on the hole created!", true, user, nullptr, nullptr, TO_CHAR);
                        act("The water collapses in on the hole create!", true, user, nullptr, nullptr, TO_ROOM);
                        break;
                }
            }
            if (SECT(IN_ROOM(user)) == SECT_WATER_SWIM || SECT(IN_ROOM(user)) == SECT_WATER_NOSWIM) {
                switch (rand_number(1, 3)) {
                    case 1:
                        act("A huge column of water erupts from the impact!", true, user, nullptr, nullptr, TO_CHAR);
                        act("A huge column of water erupts from the impact!", true, user, nullptr, nullptr, TO_ROOM);
                        break;
                    case 2:
                        act("The impact briefly causes a swirling vortex of water!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The impact briefly causes a swirling vortex of water!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 3:
                        act("A huge depression forms in the water and erupts into a wave from the impact!", true, user,
                            nullptr,
                            nullptr, TO_CHAR);
                        act("A huge depression forms in the water and erupts into a wave from the impact!", true, user,
                            nullptr,
                            nullptr, TO_ROOM);
                        break;
                }
            }
            if (SECT(IN_ROOM(user)) == SECT_INSIDE) {
                impact_sound(user, "@wA loud roar is heard nearby!@n\r\n");
                switch (rand_number(1, 8)) {
                    case 1:
                        act("Debris is thrown into the air and showers down thunderously!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("Debris is thrown into the air and showers down thunderously!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 2:
                        act("The structure of the surrounding room cracks and quakes from the blast!", true, user,
                            nullptr, nullptr,
                            TO_CHAR);
                        act("The structure of the surrounding room cracks and quakes from the blast!", true, user,
                            nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 3:
                        act("Parts of the ceiling collapse, crushing into the floor!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("Parts of the ceiling collapse, crushing into the floor!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 4:
                        act("The surrounding area roars and shudders from the impact!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The surrounding area roars and shudders from the impact!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 5:
                        act("The ground shatters apart from the stress of the impact!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The ground shatters apart from the stress of the impact!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    case 6:
                        act("The walls of the surrounding room crack in the same instant!", true, user, nullptr, nullptr,
                            TO_CHAR);
                        act("The walls of the surrounding room crack in the same instant!", true, user, nullptr, nullptr,
                            TO_ROOM);
                        break;
                    default:
                        /* we want no message for the default */
                        break;
                }
            }

            user->getRoom()->modDamage((5 - count) * 5);
        }
    }


    // Hellspear
    void Hellspear::announceAttack() {
        actUser("@WYou fly up higher in the air while holding your hand above your head. Your charged ki is condensed and materialized in the grasp of your raised hand forming a spear of energy. Grinning evily you aim the spear at the ground below and throw it. As the red spear of energy slams into the ground your laughter rings throughout the area, and the @rH@Re@Dl@Wl @rS@Rp@De@Wa@wr B@rl@Ra@Ds@wt@W erupts with a roar!@n");
        actRoom("@C$n@W flies up higher in the air while holding $s hand above $s head. @C$n@W's charged ki is condensed and materialized in the grasp of $s raised hand forming a spear of energy. Grinning evily $e aims the spear at the ground below and throws it. As the red spear of energy slams into the ground $s laughter rings throughout the area, and the @rH@Re@Dl@Wl @rS@Rp@De@Wa@wr B@rl@Ra@Ds@wt@W erupts with a roar!@n");
    }

    void Hellspear::announceHitspot() {
        actUser("@R$N@r is caught by the explosion!@n");
        actVictim("@RYou are caught by the explosion!@n");
        actOthers("@R$N@r is caught by the explosion!@n");   
    }


    // LightGrenade
    void LightGrenade::announceAttack() {
        actUser("@WYou quickly bring your hands in front of your body and cup them a short distance from each other. A flash of @Ggreen @Ylight@W can be seen as your ki is condensed between your hands before a @Yg@yo@Yl@yd@Ye@yn@W orb of ki replaces the green light. You shout @r'@YLIGHT GRENADE@r'@W as the orb launches from your hands at @C$N@W!@n");
        actRoom("@C$n@W quickly brings $s hands in front of $s body and cups them a short distance from each other. A flash of @Ggreen @Ylight@W can be seen as ki is condensed between $s hands before a @Yg@yo@Yl@yd@Ye@yn@W orb of ki replaces the green light. @C$n shouts @r'@YLIGHT GRENADE@r'@W as the orb launches from $s hands at @c$N@W!@n");
    }

    void LightGrenade::announceHitspot() {
        actUser("@R$N@r is hit by the light grenade which explodes all around $m!@n");
        actVictim("@RYou are hit by the light grenade which explodes all around you!@n");
        actOthers("@R$N@r is hit by the light grenade which explodes all around $m!@n");   
    }

    void LightGrenade::attackPostprocess() {
        if (!AFF_FLAGGED(victim, AFF_FLYING) && GET_POS(victim) == POS_STANDING && rand_number(1, 4) == 4) {
            handle_knockdown(victim);
        }
    }


    // StarNova
    void StarNova::attackPreprocess() {
        if (time_info.hours <= 15) {
            calcDamage *= 1.25;
        } else if (time_info.hours <= 22) {
            calcDamage *= 1.4;
        }
    }

    void StarNova::announceAttack() {
        actUser("@WYou gather your charged energy and clench your upheld fists at either side of your body while crouching down. A hot glow of energy begins to form around your body in the shape of a sphere! Suddenly a shockwave of heat and energy erupts out into the surrounding area as your glorious @yS@Yt@Wa@wr @cN@Co@Wv@wa@W is born!@n");
        actRoom("@C$n@W gathers $s charged energy and clenches $s upheld fists at either side of $s body while crouching down. A hot glow of energy begins to form around $s body in the shape of a sphere! Suddenly a shockwave of heat and energy erupts out into the surrounding area as @C$n's@W glorious @yS@Yt@Wa@wr @cN@Co@Wv@wa@W is born!@n");
    }

    void StarNova::announceHitspot() {
        actUser("@R$N@r is caught by the explosion!@n");
        actVictim("@RYou are caught by the explosion!@n");
        actOthers("@R$N@r is caught by the explosion!@n");   
    }


    // WeaponAttack
    // Check weapon exists
    // Modify Stamina costs
    // Pre-proccess any advantages
    // Post-process on hit affects
    // Final-process dual wield recursion
    // Change damagetype
    bool WeaponAttack::checkOtherConditions() {
        if (!secondAttack) {
            weap = GET_EQ(user, WEAR_WIELD1);
        } else {
            weap = GET_EQ(user, WEAR_WIELD2);
        }

        if (!weap) {
            send_to_char(user, "You need to wield a weapon to use this, without one try punch, kick, or other no weapon attacks.\r\n");
            return false;
        }

        if (GET_OBJ_VAL(weap, VAL_WEAPON_DAMTYPE) != getWeaponType() && wtype != 5) {
            send_to_char(user, "You need to wield a " + getName() + " to use this, without one try punch, kick, or other no weapon attacks.\r\n");
            return false;
        }

        

        return true;
    }

    void WeaponAttack::executeSecond() {
        secondAttack = true;

        if(!can_grav(user)) return;
        if(!checkSkills()) return;
        if(!checkLimbs()) return;
        if(!checkEmptyHands()) return;
        if(!checkOtherConditions()) return;

        if(!getOpponent()) return;

        if(!checkCosts()) return;

        initSkill = init_skill(user, getSkillID());

        processAttack();
    }

    void WeaponAttack::chooseSecondAttack() {
        char *victim = "";
        if(!args.empty()) {
            victim = (char*)args[0].c_str();
        }

        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_PIERCE - TYPE_HIT) {
            atk::Stab a(user, victim);
            a.executeSecond();
        } else if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_SLASH - TYPE_HIT) {
            atk::Slash a(user, victim);
            a.executeSecond();
        } else if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_CRUSH - TYPE_HIT) {
            atk::Crush a(user, victim);
            a.executeSecond();
        } else if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_STAB - TYPE_HIT) {
            atk::Impale a(user, victim);
            a.executeSecond();
        } else if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) {
            atk::Shoot a(user, victim);
            a.executeSecond();
        } else {
            atk::Smash a(user, victim);
            a.executeSecond();
        }
    }

    int64_t WeaponAttack::calculateStaminaCost() {
        int64_t stamCost = 0;
        if (weap) {
            if (!IS_ANDROID(user)) {
                stamCost += GET_OBJ_WEIGHT(weap);
            } else {
                stamCost += GET_OBJ_WEIGHT(weap) * 0.25;
            }
            wielded = 1;
        }

        if (!secondAttack && wielded == 1 && GET_EQ(user, WEAR_WIELD2) && !(GET_OBJ_VAL(weap, VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT)) {
            wielded = 2;
            if (GET_SKILL_BASE(user, SKILL_DUALWIELD) >= 100) {
                dualWield = 3;
                stamCost -= stamCost * 0.30;
            } else if (GET_SKILL_BASE(user, SKILL_DUALWIELD) >= 75) {
                dualWield = 2;
                stamCost -= stamCost * 0.25;
            }
        }

        return stamCost;
    }

    void WeaponAttack::handleAccuracyModifiers() {
        if (PLR_FLAGGED(user, PLR_THANDW)) {
            currentChanceToHit += 15;
        }
    }

    void WeaponAttack::calculateDamage() {
        calcDamage = damtype(user, -1, initSkill, attPerc);
        if (OBJ_FLAGGED(weap, ITEM_WEAPLVL1)) {
            calcDamage += calcDamage * 0.05;
            wlvl = 1;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL2)) {
            calcDamage += calcDamage * 0.1;
            wlvl = 2;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL3)) {
            calcDamage += calcDamage * 0.2;
            wlvl = 3;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL4)) {
            calcDamage += calcDamage * 0.3;
            wlvl = 4;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL5)) {
            calcDamage += calcDamage * 0.5;
            wlvl = 5;
        }

        if (PLR_FLAGGED(user, PLR_THANDW)) {
            calcDamage += calcDamage * 1.2;
        }
        
        if (PLR_FLAGGED(user, PLR_THANDW) && !(GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) && !(GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT)) {
            if (GET_SKILL_BASE(user, SKILL_TWOHAND) >= 100) {
                calcDamage += calcDamage * 0.5;
            } else if (GET_SKILL_BASE(user, SKILL_TWOHAND) >= 75) {
                calcDamage += calcDamage * 0.25;
            } else if (GET_SKILL_BASE(user, SKILL_TWOHAND) >= 50) {
                calcDamage += calcDamage * 0.1;
            }
        }

    }

    void WeaponAttack::postProcess() {

        if ((GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) && (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT)) {
            damage_weapon(user, weap, victim);
        }
        if (!IS_NPC(user)) {
            if (PLR_FLAGGED(user, PLR_THANDW)) {
                if (!GET_SKILL(user, SKILL_TWOHAND) && slot_count(user) + 1 <= GET_SLOTS(user)) {
                    int numb = rand_number(10, 15);
                    SET_SKILL(user, SKILL_TWOHAND, numb);
                    send_to_char(user, "@GYou learn the very basics of two-handing your weapon!@n\r\n");
                } else {
                    improve_skill(user, SKILL_TWOHAND, 0);
                }
            }
        }
        if (!secondAttack && GET_EQ(user, WEAR_WIELD2)) {
            if (!GET_SKILL(user, SKILL_DUALWIELD) && slot_count(user) + 1 <= GET_SLOTS(user) &&
                (GET_OBJ_TYPE(GET_EQ(user, WEAR_WIELD2)) != ITEM_LIGHT)) {
                int numb = rand_number(10, 15);
                SET_SKILL(user, SKILL_DUALWIELD, numb);
                send_to_char(user, "@GYou learn the very basics of dual-wielding!@n\r\n");
            } else {
                improve_skill(user, SKILL_DUALWIELD, 0);
            }
            if (victim != nullptr && GET_HIT(victim) > 1 && axion_dice(0) < (GET_SKILL(user, SKILL_DUALWIELD)) &&
                GET_EQ(user, WEAR_WIELD1)) {
                chooseSecondAttack();
            }
        }
    }


    // Stab
    void Stab::attackPreprocess() {
        if (!FIGHTING(user) && backstab(user, victim, wlvl, calcDamage)) {
            if (!secondAttack && victim != nullptr && GET_HIT(victim) > 1 && axion_dice(0) < (GET_SKILL(user, SKILL_DUALWIELD)) &&
                GET_EQ(user, WEAR_WIELD1) && GET_EQ(user, WEAR_WIELD2)) {
                chooseSecondAttack();
            }
            return;
        }
        calcDamage += (calcDamage * 0.01) * (GET_DEX(user) * 0.5);
    }

    void Stab::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "stomach";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou pierce @C$N's@W " + loc + "!@n");
        actVictim("@c$n@W pierces your " + loc + "!@n");
        actOthers("@c$n@W pierces @C$N's@W " + loc + "!@n");   
    }


    // Slash
    void Slash::attackPreprocess() {
        beforepl = GET_HIT(victim);
        if (IS_KONATSU(user)) {
            calcDamage += calcDamage * .25;
        }
    }

    void Slash::attackPostprocess() {
        if (beforepl - GET_HIT(victim) >= (victim->getMaxPL()) * 0.025) {
            cut_limb(user, victim, wlvl, hitspot);
        }
    }

    void Slash::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                if (!IS_NPC(user) && PLR_FLAGGED(user, PLR_THANDW) && GET_SKILL_BASE(user, SKILL_TWOHAND) >= 100) {
                    double mult = calc_critical(user, 0);
                    mult += 1.0;
                    calcDamage *= mult;
                } else {
                    calcDamage *= calc_critical(user, 0);
                }
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void Slash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou slash @C$N@W across the " + loc + "!@n");
        actVictim("@c$n@W slashes you across the " + loc + "!@n");
        actOthers("@c$n@W slashes @C$N@W across the " + loc + "!@n");   
    }


    // Crush
    void Crush::attackPreprocess() {

        if (PLR_FLAGGED(user, PLR_THANDW) && !(GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) && !(GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT)) {
            switch (wlvl) {
                case 1:
                    calcDamage += calcDamage * 0.04;
                    break;
                case 2:
                    calcDamage += calcDamage * 0.08;
                    break;
                case 3:
                    calcDamage += calcDamage * 0.12;
                    break;
                case 4:
                    calcDamage += calcDamage * 0.2;
                    break;
                case 5:
                    calcDamage += calcDamage * 0.25;
                    break;
            }
        
            
        }
        
        if (initSkill >= 100)
            calcDamage += calcDamage * 0.04;
        else if (initSkill >= 50)
            calcDamage += calcDamage * 0.1;
        
    }

    void Crush::attackPostprocess() {
        club_stamina(user, victim, wlvl, calcDamage);
    }

    void Crush::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou crush @C$N's@W " + loc + "!@n");
        actVictim("@c$n@W crushes your " + loc + "!@n");
        actOthers("@c$n@W crushes @C$N's@W " + loc + "@n");   
    }


    // Impale
    void Impale::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou stab @C$N's@W " + loc + "!@n");
        actVictim("@c$n@W stabs your " + loc + "!@n");
        actOthers("@c$n@W stabs @C$N's@W " + loc + "!@n");   
    }


    // Shoot
    bool Shoot::checkOtherConditions() {
        if (!GET_EQ(user, WEAR_WIELD1) && !GET_EQ(user, WEAR_WIELD2)) {
            send_to_char(user, "You need to wield a weapon to use this, without one try punch, kick, or other no weapon attacks.\r\n");
            return false;
        }

        if (GET_OBJ_VAL(GET_EQ(user, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != getWeaponType()) {
            send_to_char(user, "You need to wield a " + getName() + " to use this, without one try punch, kick, or other no weapon attacks.\r\n");
            return false;
        }
        

        int guncost = 1;
        struct obj_data *weap = nullptr;
        if (GET_EQ(user, WEAR_WIELD1)) {
            weap = GET_EQ(user, WEAR_WIELD1);
        } else {
            weap = GET_EQ(user, WEAR_WIELD2);
        }


        if (OBJ_FLAGGED(weap, ITEM_WEAPLVL5)) {
            guncost = 12;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL4)) {
            guncost = 6;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL3)) {
            guncost = 4;
        } else if (OBJ_FLAGGED(weap, ITEM_WEAPLVL2)) {
            guncost = 2;
        }
        if (GET_GOLD(user) < guncost) {
            send_to_char(user, "You do not have enough zenni. You need %d zenni per shot for that level of gun.\r\n",
                         guncost);
            return false;
        } else {
            user->mod(CharMoney::Carried, -guncost);
        }
        return true;

    }

    void Shoot::handleAccuracyModifiers() {
        if (PLR_FLAGGED(user, PLR_THANDW)) {
            currentChanceToHit += 15;
        }

        if (dualWield >= 2) {
            currentHitProbability += currentHitProbability * 0.1;
        }
    }

    void Shoot::handleHitspot() {
        if (PLR_FLAGGED(user, PLR_THANDW)) {
            if (hitspot != 2 && boom_headshot(user)) {
                hitspot = 2;
                send_to_char(user, "@GBoom headshot!@n\r\n");
            }
        }
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                calcDamage *= calc_critical(user, 0);
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void Shoot::calculateDamage() {
        calcDamage = gun_dam(user, wlvl);
    }

    void Shoot::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "head";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou blast @C$N's@W " + loc + "!@n");
        actVictim("@c$n@W blasts your " + loc + "!@n");
        actOthers("@c$n@W blasts @C$N's@W " + loc + "!@n");   
    }


    // Smash
    void Smash::attackPreprocess() {
        
        if (initSkill >= 100) {
            calcDamage += calcDamage * 0.5;
            wlvl = 5;
        } else if (initSkill >= 50) {
            calcDamage += calcDamage * 0.2;
            wlvl = 3;
        }
        
    }

    void Smash::handleHitspot() {
        switch(hitspot) {
            case 1:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 2:
                if (initSkill >= 100) {
                    double mult = calc_critical(user, 0);
                    mult += 1.0;
                    calcDamage *= mult;
                } else {
                    calcDamage *= calc_critical(user, 0);
                }
                break;
            case 3:
                if (GET_BONUS(user, BONUS_SOFT)) 
                    calcDamage *= calc_critical(user, 2);
                break;
            case 4:
            case 5:
                calcDamage *= calc_critical(user, 1);
                break;
        }
    }

    void Smash::announceHitspot() {
        std::string loc = "body";
        switch(hitspot) {
            case 1:
                loc = "chest";
                break;
            case 2:
                loc = "face";
                break;
            case 3:
                loc = "gut";
                break;
            case 4:
                loc = "arm";
                break;
            case 5:
                loc = "leg";
                break;
        }

        actUser("@WYou whack @C$N@W in the " + loc + "!@n");
        actVictim("@c$n@W whacks you in the " + loc + "!@n");
        actOthers("@c$n@W whacks @C$N@W in the " + loc + "!@n");   
    }



}