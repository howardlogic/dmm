/*****************************************************************************
 * es51922.h
 * May 10, 2016
 *
 * Copyright 2016 - Howard Logic, LLC
 * All Rights Reserved
 *
 *****************************************************************************/

#ifndef _ES51922_H_
#define _ES51922_H_

#include <stdint.h>

/* Parser for the the ES51922 IC serial protocol */

class ES51922 {
        public:
                enum Mode {
                        ModeInvalid,
                        ModeVolts,
                        ModeAmps,
                        ModeOhms,
                        ModeDiode,
                        ModeHZ,
                        ModeFarads,
                        ModeConductance,
                        ModeDuty
                };

                enum PowerType {
                        PowerTypeInvalid,
                        PowerTypeNone,
                        PowerTypeAC,
                        PowerTypeDC
                };

                enum Peak {
                        PeakInvalid,
                        PeakNone,
                        PeakMin,
                        PeakMax
                };

                enum Error {
                        ErrorInvalid,
                        ErrorNone,
                        ErrorOverload,
                        ErrorUnderload
                };

                // Returns the buffer size (in bytes) required to capture one measurement "packet"
                static int requiredBufferSize();
                
                // Initializes the object and resets() to "invalid" state.
                ES51922();

                // Resets the object to "invalid" state.
                void reset();

                // Parses a "packet" and updates the object's state if valid.
                bool parse(const uint8_t *buf, int size);

                double value() const;
                Mode mode() const;
                PowerType powerType() const;
                Error error() const;
                Peak peak() const;
                bool hold() const;
                bool batteryLow() const;
                bool relative() const;
                bool autoRange() const;

                // Converts the current state to a string.
                // Returns the length of the string that was created
                int toString(char *buf, int maxSize);

        private:
                class State {
                        public:
                                double          value;
                                Mode            mode;
                                PowerType       powerType;
                                Error           error;
                                Peak            peak;
                                bool            hold;
                                bool            batteryLow;
                                bool            relative;
                                bool            autoRange;

                                void reset();
                };
                State   _state;
};


#endif /* ifndef _ES51922_H_ */


