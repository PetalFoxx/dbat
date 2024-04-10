

#include "dbat/structs.h"
#include <algorithm>
#include <dbat/utils.h>

int findItemApplySlot(obj_data *pObject, int location) {
    for (int i = 0; i < MAX_OBJ_AFFECT ; i++) {
        if (pObject->affected[i].location == location || pObject->affected[i].location == 0)
            return i;
    }

    return 0;
}

static card refine = {"Refine", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int bonus = APPLY_AC;

        int slot = findItemApplySlot(task->pObject, bonus);
        int mod = 0;
        if(task->pObject->affected[slot].modifier == 0) {
            mod = task->pObject->level * task->pObject->level;
        } else {
            mod = task->pObject->affected[slot].modifier * 0.1;
        }

        task->pObject->affected[slot].modifier += mod;
        task->pObject->affected[slot].location = bonus;

        task->pObject->cost *= 1.2; 

        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;
        
        return false;},
    "You focus on improving the item and ensuring all the issues are worked out.\r\n",
    "With focus $n starts to smooth out the minute details, ensuring that the item is in perfect condition.\r\n",
    false
};

static card tranquility = {"Tranquility", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        task->improvementRounds--;
        return true;},
    "You feel a breath of tranquility as you work, finding your flow and moving through each motion with greater precision.\r\n",
    "With a serene expression, the $n's hands moved deftly over their project, each precise motion a testament to a mind at peace amidst the creative flow.\r\n",
    true
};

static card basicAttr = {"Basic Enhancement (Attributes)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 6;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_STR;
                break;
            case 1:
                bonus = APPLY_DEX;
                break;
            case 2:
                bonus = APPLY_INT;
                break;
            case 3:
                bonus = APPLY_WIS;
                break;
            case 4:
                bonus = APPLY_CON;
                break;
            case 5:
                bonus = APPLY_CHA;
                break;
        }

        rand = std::rand() % 3 + 1;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += rand;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You feel an ember of your passion spark in the item as you work.\r\n",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};

static card basicTrain = {"Basic Enhancement (Training)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 3;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_PL_GAIN_MULT;
                break;
            case 1:
                bonus = APPLY_KI_GAIN_MULT;
                break;
            case 2:
                bonus = APPLY_VITALS_GAIN_MULT;
                break;
            
        }

        double increase = 0.1;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += increase;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You feel an ember of your skill spark in the item as you work.\r\n",
    "The $n's eyes sparkle with an understanding, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};

static card improvedAttr = {"Improved Enhancement (Attributes)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 6;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_STR;
                break;
            case 1:
                bonus = APPLY_DEX;
                break;
            case 2:
                bonus = APPLY_INT;
                break;
            case 3:
                bonus = APPLY_WIS;
                break;
            case 4:
                bonus = APPLY_CON;
                break;
            case 5:
                bonus = APPLY_CHA;
                break;
        }

        rand = std::rand() % 3 + 2;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += rand;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You feel a breadth of your passion spark in the item as you work.\r\n",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};

static card improvedTrain = {"Improved Enhancement (Training)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 3;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_PL_GAIN_MULT;
                break;
            case 1:
                bonus = APPLY_KI_GAIN_MULT;
                break;
            case 2:
                bonus = APPLY_VITALS_GAIN_MULT;
                break;
            
        }

        double increase = 0.15;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += increase;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You feel a breadth of knowledge spark in the item as you work.",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};
static card expertAttr = {"Expert Enhancement (Attributes)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 6;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_STR;
                break;
            case 1:
                bonus = APPLY_DEX;
                break;
            case 2:
                bonus = APPLY_INT;
                break;
            case 3:
                bonus = APPLY_WIS;
                break;
            case 4:
                bonus = APPLY_CON;
                break;
            case 5:
                bonus = APPLY_CHA;
                break;
        }

        rand = std::rand() % 3 + 3;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += rand;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You feel a torrent of your passion spark in the item as you work.\r\n",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};
static card expertTrain = {"Expert Enhancement (Training)", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 3;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_PL_GAIN_MULT;
                break;
            case 1:
                bonus = APPLY_KI_GAIN_MULT;
                break;
            case 2:
                bonus = APPLY_VITALS_GAIN_MULT;
                break;
            
        }

        double increase = 0.2;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += increase;

        return true;},
    "You feel a library of knowledge spark in the item as you work.",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};

static card tuffleArtisany = {"Tuffle Artisany", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int rand = std::rand() % 6;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_PHYS_DAM_PERC;
                break;
            case 1:
                bonus = APPLY_KI_DAM_PERC;
                break;
            case 2:
                bonus = APPLY_DEFENSE_PERC;
                break;
            case 3:
                bonus = APPLY_PL_MULT;
                break;
            case 4:
                bonus = APPLY_KI_MULT;
                break;
            case 5:
                bonus = APPLY_LF_MULT;
                break;
            
        }

        double inc = 0.1;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += inc;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You shape the item with inhuman perfection.\r\n",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    false
};
static card tuffleIngenuity = {"Tuffle Ingenuity", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        task->improvementRounds -= 2;

        int rand = std::rand() % 6;
        int bonus = 1;
        switch(rand) {
            case 0:
                bonus = APPLY_PHYS_DAM_PERC;
                break;
            case 1:
                bonus = APPLY_KI_DAM_PERC;
                break;
            case 2:
                bonus = APPLY_DEFENSE_PERC;
                break;
            case 3:
                bonus = APPLY_PL_MULT;
                break;
            case 4:
                bonus = APPLY_KI_MULT;
                break;
            case 5:
                bonus = APPLY_LF_MULT;
                break;
            
        }

        double inc = 0.15;

        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].location = bonus;
        task->pObject->affected[findItemApplySlot(task->pObject, bonus)].modifier += inc;
        
        if(task->improvementRounds++ * 20 + (axion_dice(0) / 6) <= GET_SKILL(ch, SKILL_BUILD))
            return true;

        return false;},
    "You improve the item with incredible acuity.\r\n",
    "The $n's eyes sparkle with a passion, each careful touch on the project infusing it with a vibrant essence of their deep, fervent love for their craft.\r\n",
    true,
};
static card reflection = {"Reflection", 
    [](struct char_data *ch) {
        
        return true;},
    "",
    "",
    false
};
static card refraction = {"Refraction", 
    [](struct char_data *ch) {
        //GET_OBJ_COST(pObject) += GET_OBJ_COST(pObject) * 0.12;
        return true;},
    "",
    "",
    false
};
static card timePasses = {"Time Passes", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;
        task->improvementRounds++;
        
        return true;},
    "Time moves on, draining your will to continue..\r\n",
    "",
    false
};

static card comprehension = {   "Comprehension", 
    [](struct char_data *ch) {
        craftTask* task = &ch->craftingTask;

        int64_t pl = (GET_CON(ch) / 2) * Random::get<double>(0.8, 1.2) * ch->getPotential();
        int64_t ki = (GET_WIS(ch) / 2) * Random::get<double>(0.8, 1.2) * ch->getPotential();
        int64_t st = (GET_CON(ch) / 2) * Random::get<double>(0.8, 1.2) * ch->getPotential();

        pl *= (1 + ch->getAffectModifier(APPLY_PL_GAIN_MULT)) * (1 + ch->getAffectModifier(APPLY_VITALS_GAIN_MULT));
        ki *= (1 + ch->getAffectModifier(APPLY_KI_GAIN_MULT)) * (1 + ch->getAffectModifier(APPLY_VITALS_GAIN_MULT));
        st *= (1 + ch->getAffectModifier(APPLY_ST_GAIN_MULT)) * (1 + ch->getAffectModifier(APPLY_VITALS_GAIN_MULT));

        if(pl > (ch->getBasePL() / 10)) pl = ch->getBasePL() / 10;
        if(ki > (ch->getBaseKI() / 10)) ki = ch->getBaseKI() / 10;
        if(st > (ch->getBaseST() / 10)) st = ch->getBaseST() / 10;

        send_to_char(ch, "@D[@Y+ @R%s @rPL@D]@n\r\n", add_commas(pl).c_str());
        ch->gainBasePL(pl);
        send_to_char(ch, "@D[@Y+ @B%s @bKI@D]@n\r\n", add_commas(ki).c_str());
        ch->gainBasePL(ki);
        send_to_char(ch, "@D[@Y+ @G%s @gST@D]@n\r\n", add_commas(st).c_str());
        ch->gainBasePL(st);

        task->improvementRounds++;
        
        return true;},
    "You realise, there isn't so much difference between forging your body and creating something new.",
    "As the $n's hands worked skillfully on their project, a moment of clarity dawned, soon mirrored in the intricate details of their creation.",
    false
};

static std::vector<struct card> cards = {
    refine,
    tranquility,
    basicAttr,
    basicTrain,
    improvedAttr,
    improvedTrain,
    expertAttr,
    expertTrain,
    tuffleArtisany,
    tuffleIngenuity,
    reflection,
    refraction,
    timePasses,
    comprehension
};


static card blank = {"Blank", [](struct char_data *ch) {return true;},
            "",
            "",
            false};


void deck::shuffleDeck() {
    if(deck.empty()) {
        return;
    }

    random_shuffle(std::begin(deck) , std::end(deck));
}

void deck::discardCard(std::string cardName) {
    discardCard(findCard(cardName));
}

void deck::discardCard(card cardMove) {
    card cardNew = cardMove; 
    
    if(deck.size() > 0) {
        int i = 0;
        bool found = false;
        while((!found) && i < deck.size()) {
            if (deck.at(i).name == cardMove.name)
                found = true;
            i++;
        }

        if(found) {
            deck.erase(deck.begin() + i);
            discard.push_back(cardNew);
        }
    }
}

card deck::findCard(std::string cardName) {
    for (card found : cards) {
        if(found.name == cardName)
            return found;
    }

    return blank;
}

bool deck::playTopCard(char_data* ch) {
    bool continueDeck = true;
    shuffleDeck();
    card topDeck = deck.front();
    act(topDeck.playerAnnounce.c_str(), true, ch, nullptr, nullptr, TO_CHAR);
    act(topDeck.roomAnnounce.c_str(), true, ch, nullptr, nullptr, TO_ROOM);
    continueDeck = topDeck.effect(ch);

    if(topDeck.discard == true)
        discardCard(topDeck);
        
    return continueDeck;

}

void deck::addCardToDeck(std::string cardName, int num = 1) {
    addCardToDeck(findCard(cardName), num);
}

void deck::addCardToDeck(card toAdd, int num = 1) {
    for(int i = 0; i < num; i++) {
        deck.push_back(toAdd);
    }
}

void deck::removeCard(std::string cardName) {
    removeCard(findCard(cardName));
}

void deck::removeCard(card toRem) {
    if(deck.size() > 0) {
        int i = 0;
        bool found = false;
        while((!found) && i < deck.size()) {
            if (deck.at(i).name == toRem.name)
                found = true;
            i++;
        }

        if(found) {
            deck.erase(deck.begin() + i);
        }
    }
}

void deck::initDeck(char_data* ch) {
    deck.empty();

    int skill = GET_SKILL(ch, SKILL_BUILD);
    if(skill < 40) {
        addCardToDeck(basicAttr, 2);
        addCardToDeck(basicTrain, 3);
        addCardToDeck(reflection);
        addCardToDeck(timePasses, 3);
        addCardToDeck(tranquility);
    } else if (skill < 80) {
        addCardToDeck(basicAttr, 2);
        addCardToDeck(improvedAttr);
        addCardToDeck(basicTrain);
        addCardToDeck(refine);
        addCardToDeck(improvedTrain);
        addCardToDeck(reflection);
        addCardToDeck(timePasses, 2);
        addCardToDeck(tranquility);
    } else if (skill >=80) {
        addCardToDeck(improvedAttr, 2);
        addCardToDeck(expertAttr);
        addCardToDeck(improvedTrain);
        addCardToDeck(refine);
        addCardToDeck(expertTrain);
        addCardToDeck(reflection);
        addCardToDeck(timePasses, 2);
        addCardToDeck(tranquility);
    }

    if(ch->get(CharAttribute::Wisdom) >= 70)
        addCardToDeck(reflection);

    if(ch->get(CharAttribute::Intelligence) >= 50)
        removeCard(timePasses);


    if(IS_TRUFFLE(ch)) {
        addCardToDeck(tuffleArtisany);
        addCardToDeck(comprehension);
    }

    if(ch->transforms.contains(FormID::AscendFirst)) {
        removeCard(timePasses);
        removeCard(timePasses);
    }

    if(ch->transforms.contains(FormID::AscendSecond)) {
        addCardToDeck(tuffleIngenuity);
        addCardToDeck(tuffleArtisany);
    }

    if(ch->transforms.contains(FormID::AscendThird)) {
        removeCard(improvedAttr);
        addCardToDeck(tuffleIngenuity);
        addCardToDeck(reflection);
        addCardToDeck(refine);
    }

    shuffleDeck();

}