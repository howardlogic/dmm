/*****************************************************************************
 * es51922.cpp
 * May 10, 2016
 *
 * Copyright 2016 - Howard Logic, LLC
 * All Rights Reserved
 *
 *****************************************************************************/

#include <stdio.h>
#include "es51922.h"

#define DEBUG_ES51822

#ifdef DEBUG_ES51822
#define DEBUG(x, ...) fprintf(stderr, "%s:%d " x, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG(x, ...)
#endif
        
static const char *modeName[] = {
        "INVAL",
        "V",
        "A",
        "Ohm",
        "Diode",
        "HZ",
        "F",
        "Cond",
        "% Duty"
};

static double findBestPrefix(double val, const char **prefix) {
        const char *pfx = NULL;
        if(val      >= 1000000000.0) {
                val /= 1000000000.0;
                pfx = "G";
        } else if(val >= 1000000.0) {
                val   /= 1000000.0;
                pfx = "M";
        } else if(val >= 1000.0) {
                val   /= 1000.0;
                pfx = "K";
        } else if(val >= 1.0) {
                pfx = "";
        } else if(val >= 0.001) {
                val   /= 0.001;
                pfx = "m";
        } else if(val >= 0.000001) {
                val   /= 0.000001;
                pfx = "u";
        } else if(val >= 0.000000001) {
                val   /= 0.000000001;
                pfx = "n";
        } else if(val >= 0.000000000001) {
                val   /= 0.000000000001;
                pfx = "p";
        }
        if(pfx == NULL) pfx = "";
        if(prefix) *prefix = pfx;
        return val;
}

void ES51922::State::reset() {
        value = 0.0;
        mode = ModeInvalid;
        powerType = PowerTypeInvalid;
        error = ErrorInvalid;
        hold = false;
        batteryLow = false;
        relative = false;
        peak = PeakInvalid;
        autoRange = false;
}

int ES51922::requiredBufferSize() {
        return 14;
}

ES51922::ES51922() {
        _state.reset();
}

int ES51922::toString(char *buf, int maxSize) {
        char attr[64] = { 0 };
        const char *powstr = NULL;
        const char *prefix = NULL;
        switch(_state.powerType) {
                case PowerTypeAC: powstr = " AC"; break;
                case PowerTypeDC: powstr = " DC"; break;
                default: powstr = ""; break;
        }
        double val = findBestPrefix(_state.value, &prefix);
        int attrLen = snprintf(attr, sizeof(attr), "%s", _state.autoRange ? "[AUTO]" : "[MAN]");
        if(_state.hold) attrLen += snprintf(attr + attrLen, sizeof(attr) - attrLen, " [HOLD]");
        if(_state.batteryLow) attrLen += snprintf(attr + attrLen, sizeof(attr) - attrLen, " [BAT_LOW]");
        if(_state.relative) attrLen += snprintf(attr + attrLen, sizeof(attr) - attrLen, " [REL]");
        if(_state.peak > PeakNone) attrLen += snprintf(attr + attrLen, sizeof(attr) - attrLen, " %s", 
                        _state.peak == PeakMin ? "[MIN]" : "[MAX]");
        if(_state.error > ErrorNone) attrLen += snprintf(attr + attrLen, sizeof(attr) - attrLen, " %s", 
                        _state.error == ErrorOverload ? "[OVER]" : "[UNDER]");
        return snprintf(buf, maxSize, "%.5lf %s%s%s %s", 
                        val,
                        prefix,
                        modeName[_state.mode],
                        powstr,
                        attr);
}

bool ES51922::parse(const uint8_t *data, int size) {
        State state;

        if(size < requiredBufferSize()) {
                DEBUG("Buffer size is too small (%d)\n", size);
                return false;
        }
	if((data[0] & 0x30) != 0x30 || (data[12] != 0x0d) || (data[13] != 0x0a)) {
                DEBUG("Buffer doesn't look like a valid packet\n");
                return false;
        }

        int value = 0;
        for(int i = 0; i < 5; i++) {
                uint8_t v = data[i + 1]; // data[1..5] hold the ASCII digits of the value
                if(v < 48 || v > 57) { // value should be ascii "0" .. "9"
                        DEBUG("Digit %d isn't ascii '0' .. '9' (0x%02X)\n", i + 1, (unsigned int)data[i + 1]);
                        return false;
                }
                v -= 48; // Convert ascii value to decimal
                value = (value * 10) + v; // Multiply the current rval by 10 and add the new decimal place
        }
        if(data[7]  & 0x04) value *= -1;
        state.value = value;

	state.batteryLow = (data[7]  & 0x02);
	state.relative   = (data[8]  & 0x02);
	state.hold       = (data[11] & 0x02);
	state.autoRange  = (data[10] & 0x02);

	if(data[10] & 0x08) state.powerType = PowerTypeDC;
	else if(data[10] & 0x04) state.powerType = PowerTypeAC;
        else state.powerType = PowerTypeNone;

        if(data[7] & 0x01) state.error = ErrorOverload;
	else if(data[9] & 0x08) state.error = ErrorUnderload;
	else state.error = ErrorNone;

	if(data[9] & 0x04) state.peak = PeakMax;
	else if(data[9] & 0x02) state.peak = PeakMin;
        else state.peak = PeakNone;

        double mult = 1.0;
        if(data[7] & 0x08) {
                state.mode = ModeDuty;
                switch(data[0]) {
                        case '0': mult = 1e-1; break;
                        default:
                                  DEBUG("invalid mult 0x%02X\n", data[0]);
                                  return false;
                                  break;
                }

        } else if(data[10] & 0x01) {
		state.mode = ModeHZ;
                switch(data[0]) {
                        case '0': mult = 1e-2; break;
                        case '1': mult = 1e-1; break;
                        case '3': mult = 1.0; break;
                        case '4': mult = 1e1; break;
                        case '5': mult = 1e2; break;
                        case '6': mult = 1e3; break;
                        case '7': mult = 1e4; break;
                        default:
                                  DEBUG("invalid mult 0x%02X\n", data[0]);
                                  return false;
                                  break;
                }

        } else switch(data[6]) {
        	case '0':
                        state.mode = ModeAmps;
                        switch(data[0]) {
                                case '0': mult = 1e-3; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

	        case '1':
		        state.mode = ModeDiode;
		        break;

        	case '2':
                        state.mode = ModeHZ;
                        switch(data[0]) {
                                case '0': mult = 1e-2; break;
                                case '1': mult = 1e-1; break;
                                case '3': mult = 1.0; break;
                                case '4': mult = 1e1; break;
                                case '5': mult = 1e2; break;
                                case '6': mult = 1e3; break;
                                case '7': mult = 1e4; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

                case '3':
                        state.mode = ModeOhms;
                        switch(data[0]) {
                                case '0': mult = 1e-2; break;
                                case '1': mult = 1e-1; break;
                                case '2': mult = 1.0; break;
                                case '3': mult = 1e1; break;
                                case '4': mult = 1e2; break;
                                case '5': mult = 1e3; break;
                                case '6': mult = 1e4; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

	        case '5':
		        state.mode = ModeConductance;
		        break;

        	case '6':
		        state.mode = ModeFarads;
                        switch(data[0]) {
                                case '0': mult = 1e-12; break;
                                case '1': mult = 1e-11; break;
                                case '2': mult = 1e-10; break;
                                case '3': mult = 1e-9; break;
                                case '4': mult = 1e-8; break;
                                case '5': mult = 1e-7; break;
                                case '6': mult = 1e-6; break;
                                case '7': mult = 1e-5; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

        	case 0x3B:
                        state.mode = ModeVolts;
                        switch(data[0]) {
                                case '0': mult = 1e-4; break;
                                case '1': mult = 1e-3; break;
                                case '2': mult = 1e-2; break;
                                case '3': mult = 1e-1; break;
                                case '4': mult = 1e-5; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

                case 0x3D:
                        state.mode = ModeAmps;
                        switch(data[0]) {
                                case '0': mult = 1e-8; break;
                                case '1': mult = 1e-7; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

	        case 0x3F:
                        state.mode = ModeAmps;
                        switch(data[0]) {
                                case '0': mult = 1e-6; break;
                                case '1': mult = 1e-5; break;
                                default:
                                        DEBUG("invalid mult 0x%02X\n", data[0]);
                                        return false;
                                        break;
                        }
                        break;

	        default:
		        DEBUG("unknown data[6]: %02X\n", (unsigned int)data[6]);
                        return false;
	}
        state.value *= mult;
        _state = state;
        return true;
}

