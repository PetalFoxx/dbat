#pragma once
#include "spells.h"

struct char_data;

namespace atk {
    // If there is no entry for a skill, then the attack doesn't require a skill.
    extern std::map<int, std::vector<std::pair<int, int>>> minimumSkillRequired;

    extern std::map<int, std::vector<std::pair<int, int>>> trainSkillOnSuccess;
    extern std::map<int, std::vector<std::pair<int, int>>> trainSkillOnFailure;

    extern std::vector<int> comboSkills;

    enum class Result {
        Landed,
        Missed,
        Canceled
    };

    enum class DefenseResult {
        Missed,
        Perfect_Dodged,
        Blocked,
        Dodged,
        Parried,
        Failed
    };

    struct Attack {
        Attack(struct char_data *ch, char *arg);
        virtual int getSkillID() = 0;
        virtual int getAtkID() = 0; // used for damtype and roll_hitloc
        virtual std::string getName() = 0;
        virtual std::string getBodyPart() { return "body"; }
        virtual void execute();
        virtual void initStats();
        virtual int autoTrainSkillID() { return 0;};

        virtual Result doAttack();
        virtual DefenseResult attackOutcome(char_data*, char_data*, int, bool);
        virtual DefenseResult calculateDefense();
        virtual Result attackCharacter();
        virtual Result attackObject();

        virtual void processAttack();
        virtual void calculateDamage();
        virtual void attackPreprocess();
        virtual void attackPostprocess();
        virtual void postProcess();
        virtual void doUserCost();
        virtual void onLanded();
        virtual void onMissed();
        virtual void onCanceled();
        virtual void onLandedOrMissed();

        virtual std::optional<int> limbsToCheck();
        virtual std::optional<int> hurtInjuredLimbs();
        virtual void hurtLimb(int limb);
        virtual bool getOpponent();
        virtual bool checkLimbs();
        virtual bool checkSkills();
        virtual bool checkOtherConditions();
        virtual bool needEmptyHands() {return false;};
        virtual bool checkEmptyHands();
        virtual bool checkCosts();
        virtual bool isPhysical();
        virtual bool usesWeapon();
        virtual bool isKiAttack();

        void actUser(const std::string& msg, bool hideInvisible = true);
        void actOthers(const std::string& msg, bool hideInvisible = true);
        void actRoom(const std::string& msg, bool hideInvisible = true);
        void actVictim(const std::string& msg, bool hideInvisible = true);

        virtual int64_t calculateHealthCost();
        virtual int64_t calculateStaminaCost();
        virtual double calculateKiCost();
        virtual std::optional<int> hasCooldown();
        virtual int canKillType();
        virtual int limbhurtChance() {return 0;};
        virtual int getHoming() {return 0;};
        virtual double getKiEfficiency() {return 1;};
        virtual int getTier() {return 0;};

        virtual void doTrain(const std::vector<std::pair<int, int>>& skills);

        virtual bool canBlock();
        virtual bool canParry();
        virtual bool canDodge();
        virtual bool canZanzoken();
        virtual bool canCombo();

        virtual Result handleOtherHit();
        virtual Result handleCleanHit();
        virtual void handleHitspot();
        virtual void announceHitspot();

        virtual bool calculateDeflect();
        virtual void handleAccuracyModifiers();
        virtual Result handleMiss();
        virtual Result handleParry() = 0;
        virtual Result handleBlock() = 0;
        virtual Result handleDodge() = 0;
        virtual Result handlePerfectDodge() = 0;

        virtual int64_t calculateObjectDamage();

        virtual void announceParry() = 0;
        virtual void announceBlock() = 0;
        virtual void announceDodge() = 0;
        virtual void announceMiss() = 0;
        virtual void announceObject() = 0;

        struct char_data *user{};
        struct char_data *victim{};
        struct obj_data *obj{};
        std::string input;
        std::vector<std::string> args;

        int currentSpeedIndexCheck{0};
        int currentBlockCheck{0};
        int currentDodgeCheck{0};
        int currentParryCheck{0};
        int currentHitProbability{0};
        int currentChanceToHit{0};
        int initSkill{0};
        int hitspot{1};
        int targetLimb{1};
        int cooldownOverride{-1};
        int64_t currentHealthCost{0};
        int64_t currentStaminaCost{0};
        double currentKiCost{0};
        double attPerc{0.0};
        int64_t calcDamage{0};
        DefenseResult defenseResult{DefenseResult::Failed};

    };

    struct MeleeAttack : Attack {
        using Attack::Attack;
        int64_t calculateObjectDamage() override;
        int64_t calculateStaminaCost() override;
        bool canBlock() override {return true;};
        bool canParry() override {return true;};
        bool canDodge() override {return true;};
        bool isPhysical() override {return true;};
        bool canCombo() override {return true;};
        Result handleParry() override;
        Result handleDodge() override;
        Result handlePerfectDodge() override;
        Result handleBlock() override;
        void handleHitspot() override;

        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        //void announceHitspot() override;
    };

    struct RangedAttack : Attack {
        using Attack::Attack;
        int canKillType() override;
    };

    struct RangedKiAttack : RangedAttack {
        using RangedAttack::RangedAttack;

        bool isKiAttack() override {return true;};
        bool isPhysical() override {return false;};
        bool checkOtherConditions() override;
        double calculateKiCost() override;
        double getKiEfficiency();
        int getTier() {return 1;};
        int getHoming() {return 0;};
        Result handleParry() override;
        Result handleDodge() override;
        Result handlePerfectDodge() override;
        Result handleBlock() override;

        void announceObject() override;
        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
    };

    // atkID -1 is used for parry.
    struct HandAttack : MeleeAttack {
        using MeleeAttack::MeleeAttack;
        bool needEmptyHands() override {return true;};
        std::optional<int> limbsToCheck() override {return 0;};
        std::optional<int> hurtInjuredLimbs() override {return 0;};
    };

    struct LegAttack : MeleeAttack {
        using MeleeAttack::MeleeAttack;
        std::optional<int> limbsToCheck() override {return 1;};
        std::optional<int> hurtInjuredLimbs() override {return 1;};
    };

    struct Punch : HandAttack {
        using HandAttack::HandAttack;
        int getSkillID() override { return SKILL_PUNCH; };
        int getAtkID() override {return 0;};
        std::string getName() override { return "punch"; };
        std::string getBodyPart() override {return "hand";};
        std::optional<int> hasCooldown() override {return 4;};
        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        void announceHitspot() override;
    };

    struct Kick : LegAttack {
        using LegAttack::LegAttack;
        int getSkillID() override { return SKILL_KICK; }
        int getAtkID() override {return 1;};
        std::string getName() override { return "kick"; };
        std::string getBodyPart() override {return "foot";};
        std::optional<int> hasCooldown() override {return 5;};

        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        void announceHitspot() override;
    };

    struct Elbow : HandAttack {
        using HandAttack::HandAttack;
        int getSkillID() override { return SKILL_ELBOW; }
        int getAtkID() override {return 2;};
        std::string getName() override { return "elbow"; }
        std::string getBodyPart() override {return "elbow";};
        std::optional<int> hasCooldown() override {return 6;};

        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        void announceHitspot() override;
    };

    struct Knee : LegAttack {
        using LegAttack::LegAttack;
        int getSkillID() override { return SKILL_KNEE; }
        int getAtkID() override {return 3;};
        std::string getName() override { return "knee"; }
        std::string getBodyPart() override {return "knee";};
        std::optional<int> hasCooldown() override {return 6;};

        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        void announceHitspot() override;
    };

    struct Roundhouse : LegAttack {
        using LegAttack::LegAttack;
        int getSkillID() override { return SKILL_ROUNDHOUSE; }
        int getAtkID() override {return 4;};
        std::string getName() override { return "roundhouse"; }
        std::string getBodyPart() override {return "foot";};
        std::optional<int> hasCooldown() override;

        int limbhurtChance() override;
        void handleHitspot() override;
        void announceParry() override;
        void announceBlock() override;
        void announceDodge() override;
        void announceMiss() override;
        void announceObject() override;
        void announceHitspot() override;
    };

    struct Uppercut : HandAttack {
        using HandAttack::HandAttack;
        int getSkillID() override { return SKILL_UPPERCUT; }
        int getAtkID() override {return 5;};
        std::string getName() override { return "uppercut"; }
        std::string getBodyPart() override {return "hand";};
        std::optional<int> hasCooldown() override;
        void handleHitspot() override;

        void announceObject() override;
        void announceHitspot() override;
    };

    struct Slam : HandAttack {
        using HandAttack::HandAttack;
        int getSkillID() override { return SKILL_SLAM; }
        int getAtkID() override {return 6;};
        std::string getName() override { return "slam"; }
        std::string getBodyPart() override {return "hand";};
        std::optional<int> hasCooldown() override;
        void handleHitspot() override;

        void announceObject() override;
        void announceHitspot() override;
    };

    struct Heeldrop : LegAttack {
        using LegAttack::LegAttack;
        int getSkillID() override { return SKILL_HEELDROP; }
        int getAtkID() override {return 8;};
        std::string getName() override { return "heeldrop"; }
        std::string getBodyPart() override {return "foot";};
        std::optional<int> hasCooldown() override;
        void handleHitspot() override;

        void announceObject() override;
        void announceHitspot() override;
    };

    struct KiBall : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;
        int getSkillID() override { return SKILL_KIBALL; }
        int getAtkID() override {return 7;};
        std::string getName() override { return "kiball"; }
        std::optional<int> hasCooldown() override {return 5;};

        void processAttack() override;
        void announceHitspot() override;
    };

    struct KiBlast : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;
        int64_t record = 0;

        int getSkillID() override { return SKILL_KIBLAST; }
        int getAtkID() override {return 9;};
        std::string getName() override { return "kiblast"; }
        std::optional<int> hasCooldown() override;
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Beam : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_BEAM; }
        int getAtkID() override {return 10;};
        std::string getName() override { return "beam"; }
        std::optional<int> hasCooldown() override {return 5;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Tsuihidan : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_TSUIHIDAN; }
        int getAtkID() override {return 11;};
        int getHoming() override {return 2;}
        int getTier() override {return 2;};
        std::string getName() override { return "tsuihidan"; }
        std::optional<int> hasCooldown() override {return 5;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Renzo : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int count = 0;
        int getSkillID() override { return SKILL_RENZO; }
        int getAtkID() override {return 12;};
        std::string getName() override { return "renzo"; }
        std::optional<int> hasCooldown() override {return 5;};
        int getTier() override {return 2;};
        DefenseResult calculateDefense() override;
        double getKiEfficiency() override;
        void attackPreprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;
    };

    struct Shogekiha : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_SHOGEKIHA; }
        int getAtkID() override {return 10;};
        std::string getName() override { return "shogekiha"; }
        int getTier() override {return 2;};
        std::optional<int> hasCooldown() override;
        void attackPostprocess() override;
        Result attackObject() override;

        void announceHitspot() override;
        void announceObject() override;
    };

    struct Kamehameha : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_KAMEHAMEHA; }
        int getAtkID() override {return 13;};
        std::string getName() override { return "kamehameha"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override;
        void postProcess() override;

        void announceHitspot() override;
    };

    struct Masenko : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_MASENKO; }
        int getAtkID() override {return 14;};
        std::string getName() override { return "masenko"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPreprocess() override;

        void announceHitspot() override;
    };

    struct Dodonpa : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_DODONPA; }
        int getAtkID() override {return 15;};
        std::string getName() override { return "dodonpa"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct GalikGun : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_GALIKGUN; }
        int getAtkID() override {return 16;};
        std::string getName() override { return "galik gun"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};

        void announceHitspot() override;
    };

    struct DeathBeam : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_DEATHBEAM; }
        int getAtkID() override {return 17;};
        std::string getName() override { return "death beam"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override;
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct EraserCannon : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_ERASER; }
        int getAtkID() override {return 18;};
        std::string getName() override { return "eraser cannon"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void postProcess() override;
        void handleHitspot() override;

        void announceHitspot() override;
    };

    struct TwinSlash : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_TSLASH; }
        int getAtkID() override {return 19;};
        std::string getName() override { return "twin slash"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        bool usesWeapon() override { return true; }
        bool checkOtherConditions() override;
        void handleHitspot() override;
        void attackPreprocess() override;

        void announceHitspot() override;
    };

    struct PsychicBlast : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_PSYBLAST; }
        int getAtkID() override {return 20;};
        std::string getName() override { return "psychic blast"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Honoo : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_HONOO; }
        int getAtkID() override {return 21;};
        std::string getName() override { return "honoo"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPreprocess() override;
        void attackPostprocess() override;
        void postProcess() override;

        void announceHitspot() override;

    };

    struct DualBeam : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_DUALBEAM; }
        int getAtkID() override {return 22;};
        std::string getName() override { return "dual beam"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void handleHitspot() override;
        void processAttack() override;

        void announceHitspot() override;
    };

    struct Rogafufuken : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;
        int canKillType() override {return 0;};

        int getSkillID() override { return SKILL_ROGAFUFUKEN; }
        int getAtkID() override {return 23;};
        std::string getName() override { return "rogafufuken"; }
        int getTier() override {return 2;};
        int limbhurtChance() override;
        std::optional<int> hasCooldown() override {return 7;};

        void announceHitspot() override;
    };


    struct Kienzan : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_KIENZAN; }
        int getAtkID() override {return 25;};
        std::string getName() override { return "kienzan"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        int getHoming() override {return 2;}
        bool canBlock() override {return false;};
        bool canParry() override {return false;};
        void attackPreprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };

    struct Tribeam : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_TRIBEAM; }
        int getAtkID() override {return 26;};
        std::string getName() override { return "tribeam"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canBlock() override {return false;};
        bool canParry() override {return false;};
        void attackPreprocess() override;

        void announceHitspot() override;
    };

    struct SpecialBeamCannon : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_SBC; }
        int getAtkID() override {return 27;};
        std::string getName() override { return "special beam cannon"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canBlock() override {return false;};
        bool canParry() override {return false;};

        void announceHitspot() override;
        
    };

    struct PsychicBarrage : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_PBARRAGE; }
        int getAtkID() override {return 31;};
        std::string getName() override { return "psychic barrage"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canBlock() override {return false;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Genocide : RangedKiAttack {

    };

    struct GenkiDama : RangedKiAttack {

    };

    struct Spiritball : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_SPIRITBALL; }
        int getAtkID() override {return 39;};
        std::string getName() override { return "spiritball"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 8;};
        int getHoming() override {return 2;}

        void announceHitspot() override;

    };

    struct Deathball : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_DEATHBALL; }
        int getAtkID() override {return 38;};
        std::string getName() override { return "deathball"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 8;};
        bool canBlock() override {return false;};
        bool canParry() override {return false;};

        void announceHitspot() override;
    };

    struct PhoenixSlash : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_PSLASH; }
        int getAtkID() override {return 37;};
        std::string getName() override { return "phoenix slash"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 8;};
        bool usesWeapon() override { return true; }
        bool canParry() override {return false;};
        bool checkOtherConditions() override;
        int limbhurtChance() override;
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;

    };

    struct BigBang : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_BIGBANG; }
        int getAtkID() override {return 36;};
        std::string getName() override { return "big bang"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 8;};
        bool canParry() override {return false;};
        void handleHitspot() override;

        void announceHitspot() override;

    };

    struct ScatterShot : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_SCATTER; }
        int getAtkID() override {return 35;};
        std::string getName() override { return "scatter shot"; }
        int getTier() override {return 4;};
        bool canParry() override {return false;};
        std::optional<int> hasCooldown() override;

        void announceHitspot() override;

    };

    struct Balefire : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_BALEFIRE; }
        int getAtkID() override {return 35;};
        std::string getName() override { return "balefire"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override;
        bool canParry() override {return false;};

        void announceHitspot() override;
    };

    

    struct Hellflash : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_HELLFLASH; }
        int getAtkID() override {return 32;};
        std::string getName() override { return "hellflash"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canParry() override {return false;};
        void attackPreprocess() override;

        void announceHitspot() override;

    };

    struct DarknessDragonSlash : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_DDSLASH; }
        int getAtkID() override {return 30;};
        std::string getName() override { return "darkness dragon slash"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool checkOtherConditions() override;
        bool canParry() override {return false;};
        int limbhurtChance() override;
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;

    };

    struct CrusherBall : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_CRUSHER; }
        int getAtkID() override {return 29;};
        std::string getName() override { return "crusher ball"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canParry() override {return false;};
        void attackPostprocess() override;

        void announceHitspot() override;

    };

    struct FinalFlash : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_FINALFLASH; }
        int getAtkID() override {return 28;};
        std::string getName() override { return "final flash"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canParry() override {return false;};
        int limbhurtChance() override;

        void announceHitspot() override;
    };

    struct Kousengan : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_KOUSENGAN; }
        int getAtkID() override {return 42;};
        std::string getName() override { return "kousengan"; }
        int getTier() override {return 2;};
        std::optional<int> hasCooldown() override {return 5;};
        bool canParry() override {return false;};
        int limbhurtChance() override;
        void attackPostprocess() override;

        void announceHitspot() override;

    };

    struct FireBreath : MeleeAttack {
        using MeleeAttack::MeleeAttack;

        int getSkillID() override { return SKILL_KNEE; }
        int getAtkID() override {return 8;};
        std::string getName() override { return "fire breath"; }
        std::optional<int> hasCooldown() {return 10;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Ram : MeleeAttack {
        using MeleeAttack::MeleeAttack;

        int getSkillID() override { return SKILL_KNEE; }
        int getAtkID() override {return 8;};
        std::string getName() override { return "ram"; }
        std::optional<int> hasCooldown() {return 4;};

        void announceHitspot() override;
    };

    struct FangStrike : MeleeAttack {
        using MeleeAttack::MeleeAttack;

        int getSkillID() override { return SKILL_KNEE; }
        int getAtkID() override {return 8;};
        std::string getName() override { return "fang strike"; }
        std::optional<int> hasCooldown() {return 4;};
        void attackPreprocess() override;

        void announceHitspot() override;

    };

    struct SunderingForce : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_ZEN; }
        int getAtkID() override {return 55;};
        std::string getName() override { return "sundering force"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() {return 6;};
        bool canParry() override {return false;};
        bool canBlock() override {return false;};
        int limbhurtChance() override;

        void announceHitspot() override;
    };

    struct ZenBlade : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_ZEN; }
        int getAtkID() override {return 54;};
        std::string getName() override { return "zen blade"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        bool canParry() override {return false;};
        bool checkOtherConditions() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };

    struct MaliceBreaker : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_MALICE; }
        int getAtkID() override {return 36;};
        std::string getName() override { return "malice breaker"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 8;};
        bool canParry() override {return false;};
        void attackPreprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;
    };

    struct Bash : HandAttack {
        using HandAttack::HandAttack;

        int getSkillID() override { return SKILL_BASH; }
        int getAtkID() override {return 51;};
        std::string getName() override { return "bash"; }
        std::optional<int> hasCooldown() {return 6;};
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct SeishouEnko : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_SEISHOU; }
        int getAtkID() override {return 50;};
        std::string getName() override { return "seishou enko"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 4;};
        void attackPreprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;
    };

    struct SelfDestruct : RangedKiAttack {

    };

    struct WaterRazor : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_WRAZOR; }
        int getAtkID() override {return 47;};
        std::string getName() override { return "water razor"; }
        int64_t reduction;
        int getTier() override {return 2;};
        std::optional<int> hasCooldown() override {return 6;};
        bool canParry() override {return false;};
        bool canBlock() override {return false;};
        bool checkOtherConditions() override;
        void attackPreprocess() override;
        void attackPostprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };

    struct WaterSpike : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_WSPIKE; }
        int getAtkID() override {return 43;};
        std::string getName() override { return "water spike"; }
        int getTier() override {return 2;};
        std::optional<int> hasCooldown() override {return 6;};
        bool canParry() override {return false;};
        void attackPreprocess() override;
        void postProcess() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };
    struct KoteiruBakuha : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_KOTEIRU; }
        int getAtkID() override {return 48;};
        std::string getName() override { return "koteiru bakuha"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 7;};
        bool canParry() override {return false;};
        bool canBlock() override {return false;};
        int limbhurtChance() override;
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;
    };
    struct HellSpiral : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        int getSkillID() override { return SKILL_HSPIRAL; }
        int getAtkID() override {return 49;};
        std::string getName() override { return "hell spiral"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        bool canParry() override {return false;};
        bool canBlock() override {return false;};
        int limbhurtChance() override;

        void announceHitspot() override;
    };
    struct Spiral : RangedKiAttack {

    };
    struct StarBreaker : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;
        int64_t theft = 0;
        int64_t taken = 0;

        int getSkillID() override { return SKILL_BREAKER; }
        int getAtkID() override {return 46;};
        std::string getName() override { return "star breaker"; }
        int getTier() override {return 2;};
        std::optional<int> hasCooldown() override {return 6;};
        int limbhurtChance() override;
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;
    };

    struct Bite : MeleeAttack {
        using MeleeAttack::MeleeAttack;
        int getSkillID() override { return SKILL_PUNCH; }
        int getAtkID() override {return 8;};
        std::string getName() override { return "bite"; }
        std::optional<int> hasCooldown() {return 4;};
        void attackPostprocess() override;
        void attackPreprocess() override;

        void announceHitspot() override;
    };

    struct Headbutt : MeleeAttack {
        using MeleeAttack::MeleeAttack;
        int getSkillID() override { return SKILL_HEADBUTT; }
        int getAtkID() override {return 52;};
        std::string getName() override { return "headbutt"; }
        std::string getBodyPart() override {return "head";};
        std::optional<int> hasCooldown() override;
        void handleHitspot() override;

        void announceObject() override;
        void announceHitspot() override;
    };

    struct Tailwhip : MeleeAttack {
        using MeleeAttack::MeleeAttack;

        int getSkillID() override { return SKILL_TAILWHIP; }
        int getAtkID() override {return 56;};
        std::string getName() override { return "tailwhip"; }
        std::optional<int> hasCooldown() override {return 7;};
        std::string getBodyPart() override {return "tail";};
        bool checkOtherConditions() override;
        int limbhurtChance() override;

        void announceHitspot() override;

    };

    struct KiAreaAttack : RangedKiAttack {
        using RangedKiAttack::RangedKiAttack;

        std::vector<char_data*> targets;
        bool paidCost = false;
        bool canParry() override {return false;};
        bool canBlock() override {return false;};
        void doUserCost() override;
        bool getOpponent() override;
        void processAttack() override;
        virtual void announceAttack();

    };

    
    struct Bakuhatsuha : KiAreaAttack {
        using KiAreaAttack::KiAreaAttack;

        int getSkillID() override { return SKILL_BAKUHATSUHA; }
        int getAtkID() override {return 24;};
        std::string getName() override { return "bakuhatsuha"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPreprocess() override;

        void announceAttack() override;
        void announceHitspot() override;
    };

    struct Kakusanha : KiAreaAttack {
        using KiAreaAttack::KiAreaAttack;

        int getSkillID() override { return SKILL_KAKUSANHA; }
        int getAtkID() override {return 34;};
        std::string getName() override { return "kakusanha"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 5;};
        void attackPreprocess() override;
        void postProcess() override;

        void announceAttack() override;
        void announceHitspot() override;
    };

    struct Hellspear : KiAreaAttack {
        using KiAreaAttack::KiAreaAttack;

        int getSkillID() override { return SKILL_HELLSPEAR; }
        int getAtkID() override {return 33;};
        std::string getName() override { return "hellspear"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 5;};

        void announceAttack() override;
        void announceHitspot() override;

    };

    struct LightGrenade : KiAreaAttack {
        using KiAreaAttack::KiAreaAttack;

        int getSkillID() override { return SKILL_LIGHTGRENADE; }
        int getAtkID() override {return 57;};
        std::string getName() override { return "light grenade"; }
        int getTier() override {return 4;};
        std::optional<int> hasCooldown() override {return 9;};
        void attackPostprocess() override;

        void announceAttack() override;
        void announceHitspot() override;

    };

    struct StarNova : KiAreaAttack {
        using KiAreaAttack::KiAreaAttack;

        int getSkillID() override { return SKILL_STARNOVA; }
        int getAtkID() override {return 53;};
        std::string getName() override { return "star nova"; }
        int getTier() override {return 3;};
        std::optional<int> hasCooldown() override {return 6;};
        void attackPreprocess() override;

        void announceAttack() override;
        void announceHitspot() override;

    };


    struct WeaponAttack : MeleeAttack {
        using MeleeAttack::MeleeAttack;

        struct obj_data *weap = nullptr;
        bool secondAttack = false;
        int wielded = 0;
        int dualWield = 0;
        int wtype = 0;
        int wlvl = 0;

        bool usesWeapon() override { return true; }
        virtual int getWeaponType() {return (TYPE_PIERCE - TYPE_HIT);};
        std::string getName() override { return "dagger"; }
        int getAtkID() override {return -1;};
        std::optional<int> hasCooldown() override {return 8;};
        bool checkOtherConditions() override;
        int64_t calculateStaminaCost() override;
        void executeSecond();
        void chooseSecondAttack();
        void calculateDamage() override;
        void postProcess() override;
        void handleAccuracyModifiers() override;

    };

    //Dagger
    struct Stab : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 1;
        int getSkillID() override { return SKILL_DAGGER; }

        int getWeaponType() {return (TYPE_PIERCE - TYPE_HIT);};
        std::string getName() override { return "dagger"; }
        void attackPreprocess() override;

        void announceHitspot() override;

    };

    //Sword
    struct Slash : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 0;
        int64_t beforepl = 0;
        int getSkillID() override { return SKILL_SWORD; }

        int getWeaponType() {return (TYPE_SLASH - TYPE_HIT);};
        std::string getName() override { return "sword"; }
        void attackPreprocess() override;
        void attackPostprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };

    //Crush
    struct Crush : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 2;
        
        int getSkillID() override { return SKILL_CLUB; }

        int getWeaponType() {return (TYPE_CRUSH - TYPE_HIT);};
        std::string getName() override { return "club"; }
        void attackPreprocess() override;
        void attackPostprocess() override;

        void announceHitspot() override;

    };

    //Spear
    struct Impale : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 3;
        int getSkillID() override { return SKILL_SPEAR; }

        int getWeaponType() {return (TYPE_STAB - TYPE_HIT);};
        std::string getName() override { return "spear"; }

        void announceHitspot() override;

    };

    //Gun
    struct Shoot : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 4;
        int getSkillID() override { return SKILL_GUN; }

        int getWeaponType() {return (TYPE_BLAST - TYPE_HIT);};
        std::string getName() override { return "gun"; }
        void handleHitspot() override;
        bool checkOtherConditions() override;
        void calculateDamage() override;
        void handleAccuracyModifiers() override;
        

        void announceHitspot() override;

        int canKillType() override {return 1;};

    };

    //Other
    struct Smash : WeaponAttack {
        using WeaponAttack::WeaponAttack;

        int wtype = 5;
        int getSkillID() override { return SKILL_BRAWL; }

        int getWeaponType() {return (TYPE_PUNCH - TYPE_HIT);};
        std::string getName() override { return "weapon"; }
        void attackPreprocess() override;
        void handleHitspot() override;

        void announceHitspot() override;

    };


}
