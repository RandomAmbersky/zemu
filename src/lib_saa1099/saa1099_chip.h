#ifndef _SAA1099_CHIP_H_INCLUDED_
#define _SAA1099_CHIP_H_INCLUDED_

#include "SAASound.h"

class C_Saa1099Chip {
public:

    C_SndRenderer sndRenderer;

    C_Saa1099Chip();
    virtual ~C_Saa1099Chip();

    void WriteAddress(unsigned char reg);
    void WriteData(unsigned char data);

    void Reset(void);
    void Render(unsigned devClk);

protected:

    LPCSAASOUND chip;
};

#endif
