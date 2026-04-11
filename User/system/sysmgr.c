#include "system.h"




void systemInitMode() {
    // Hardware initialization and self-check should be performed in this mode.
}

void systemPowerupSelfCheckMode() {

}

void systemStandbyMode() {

}

void systemNormalMode() {

}

void systemSelfCheckMode() {

}


void systemUpdateMode() {

}

void systemDiagnosticMode() {

}

void systemEolMode() {

}


void systemManagerRun() {
    switch(systemGetMode()) {
        case eSYSTEM_INIT_MODE:
            systemInitMode();
            break;
        case eSYSTEM_POWERUP_SELFCHECK_MODE:
            systemPowerupSelfCheckMode();
            break;
        case eSYSTEM_STANDBY_MODE:
            systemStandbyMode();
            break;
        case eSYSTEM_NORMAL_MODE:
            systemNormalMode();
            break;
        case eSYSTEM_SELF_CHECK_MODE:
            systemSelfCheckMode();
            break;
        case eSYSTEM_UPDATE_MODE:
            systemUpdateMode();
            break;
        case eSYSTEM_DIAGNOSTIC_MODE:
            systemDiagnosticMode();
            break;
        case eSYSTEM_EOL_MODE:
            systemEolMode();
            break;
        default:
            break;
    }
}