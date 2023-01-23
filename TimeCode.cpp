/* Copyright (c) MediaArea.net SARL. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

//---------------------------------------------------------------------------
#include "TimeCode.h"
#include <sstream>
#include <limits>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
TimeCode::TimeCode ()
{
    memset(this, 0, sizeof(TimeCode));
}

//---------------------------------------------------------------------------
TimeCode::TimeCode (uint32_t Hours_, uint8_t Minutes_, uint8_t Seconds_, uint32_t Frames_, uint32_t FramesMax_, bool DropFrame_, bool MustUseSecondField_, bool IsSecondField_)
:   Hours(Hours_),
    Minutes(Minutes_),
    Seconds(Seconds_),
    Frames(Frames_),
    FramesMax(FramesMax_)
{
    if (DropFrame_)
        Flags.set(DropFrame);
    if (MustUseSecondField_)
        Flags.set(MustUseSecondField);
    if (IsSecondField_)
        Flags.set(IsSecondField);
    Flags.set(IsValid);
}

//---------------------------------------------------------------------------
TimeCode::TimeCode (int64_t Frames_, uint32_t FramesMax_, bool DropFrame_, bool MustUseSecondField_, bool IsSecondField_)
:   FramesMax(FramesMax_)
{
    if (DropFrame_)
        Flags.set(DropFrame);
    if (MustUseSecondField_)
        Flags.set(MustUseSecondField);
    if (IsSecondField_)
        Flags.set(IsSecondField);
    FromFrames(Frames_);
}

bool TimeCode::FromFrames(int64_t Frames_)
{
    if (Frames_<0)
    {
        Flags.set(IsNegative);
        Frames_=-Frames_;
    }
    else
        Flags.reset(IsNegative);

    uint64_t Dropped=Flags.test(DropFrame)?(1+FramesMax/30):0;
    uint32_t FrameRate=(uint32_t)FramesMax+1;
    uint64_t Dropped2=Dropped*2;
    uint64_t Dropped18=Dropped*18;

    uint64_t Minutes_Tens = ((uint64_t)Frames_)/(600*FrameRate-Dropped18); //Count of 10 minutes
    uint64_t Minutes_Units = (Frames_-Minutes_Tens*(600*FrameRate-Dropped18))/(60*FrameRate-Dropped2);

    Frames_+=Dropped18*Minutes_Tens+Dropped2*Minutes_Units;
    if (Minutes_Units && ((Frames_/FrameRate)%60)==0 && (Frames_%FrameRate)<Dropped2) // If Minutes_Tens is not 0 (drop) but count of remaining seconds is 0 and count of remaining frames is less than 2, 1 additional drop was actually counted, removing it
        Frames_-=Dropped2;

    int64_t HoursTemp=(((Frames_/FrameRate)/60)/60);
    if (HoursTemp>(uint32_t)-1)
    {
        Hours=(uint32_t)-1;
        Minutes=59;
        Seconds=59;
        Frames=FramesMax;
        return true;
    }
    Hours=(uint8_t)HoursTemp;
    Minutes=((Frames_/FrameRate)/60)%60;
    Seconds=(Frames_/FrameRate)%60;
    Frames=(uint32_t)(Frames_%FrameRate);
    Flags.reset(IsTime);
    Flags.set(IsValid);

    return false;
}

//---------------------------------------------------------------------------
TimeCode::TimeCode (const char* Value, size_t Length)
:   FramesMax(0)
{
    FromString(Value, Length);
}

//***************************************************************************
// Helpers
//***************************************************************************

//---------------------------------------------------------------------------
void TimeCode::PlusOne()
{
    //TODO: negative values

    if (Flags.test(HasNoFramesInfo))
        return;

    if (Flags.test(MustUseSecondField))
    {
        if (Flags.test(IsSecondField))
        {
            Frames++;
            Flags.reset(IsSecondField);
        }
        else
            Flags.set(IsSecondField);
    }
    else
        Frames++;
    if (Frames>FramesMax || !Frames)
    {
        Seconds++;
        Frames=0;
        if (Seconds>=60)
        {
            Seconds=0;
            Minutes++;

            if (Flags.test(DropFrame) && Minutes%10)
                Frames=(1+FramesMax/30)*2; //frames 0 and 1 (at 30 fps) are dropped for every minutes except 00 10 20 30 40 50

            if (Minutes>=60)
            {
                Minutes=0;
                Hours++;
                if (Hours>=24)
                {
                    Hours=0;
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
void TimeCode::MinusOne()
{
    //TODO: negative values

    if (Flags.test(HasNoFramesInfo))
        return;

    if (Flags.test(MustUseSecondField) && Flags.test(IsSecondField))
    {
        Flags.reset(IsSecondField);
        return;
    }

    bool d=Flags.test(DropFrame);
    if (!FramesMax && (!Frames || d))
        return;
    if (Flags.test(MustUseSecondField))
        Flags.set(IsSecondField);

    if (Frames && !(d && Minutes%10 && Frames<(1+FramesMax/30)*2))
    {
        Frames--;
        return;
    }

    Frames=FramesMax;
    if (!Seconds)
    {
        Seconds=59;
        if (!Minutes)
        {
            Minutes=59;
            if (!Hours)
                Hours=24;
            else
                Hours--;
        }
        else
            Minutes--;
    }
    else
        Seconds--;
}

//---------------------------------------------------------------------------
static const int32_t PowersOf10[]=
{
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000,
};
static const int PowersOf10_Size=sizeof(PowersOf10)/sizeof(int32_t);

//---------------------------------------------------------------------------
bool TimeCode::FromString(const char* Value, size_t Length)
{
    //hh:mm:ss;ff or hh:mm:ss.zzzzzzzzzSfffffffff formats
    if (Length>7
     && Value[0]>='0' && Value[0]<='9'
     && Value[1]>='0' && Value[1]<='9'
     && Value[2]==':'
     && Value[3]>='0' && Value[3]<='9'
     && Value[4]>='0' && Value[4]<='9'
     && Value[5]==':'
     && Value[6]>='0' && Value[6]<='9'
     && Value[7]>='0' && Value[7]<='9')
    {
        if (Length>8)
        {
            //hh:mm:ss.zzzzzzzzzSfffffffff format
            unsigned char c=(unsigned char)Value[8];
            if (c=='.' || c==',')
            {
                if (Length==9)
                    return true;
                int i=9;
                int32_t S=0;
                int TheoriticalMax=i+PowersOf10_Size;
                int MaxLength=Length>TheoriticalMax?TheoriticalMax:Length;
                while (i<MaxLength)
                {
                    c=(unsigned char)Value[i];
                    c-='0';
                    if (c>9)
                        break;
                    S*=10;
                    S+=c;
                    i++;
                }
                if (i==Length)
                    FramesMax=PowersOf10[i-10]-1;
                else
                {
                    c=(unsigned char)Value[i];
                    if (c!='S' && c!='/')
                        return true;
                    i++;
                    TheoriticalMax=i+PowersOf10_Size;
                    MaxLength=Length>TheoriticalMax?TheoriticalMax:Length;
                    uint32_t Multiplier=0;
                    while (i<Length)
                    {
                        c=(unsigned char)Value[i];
                        c-='0';
                        if (c>9)
                            break;
                        Multiplier*=10;
                        Multiplier+=c;
                    }
                    if (i==MaxLength && i<Length && Value[i]=='0' && Multiplier==100000000)
                    {
                        Multiplier=1000000000;
                        i++;
                    }
                    if (i<Length)
                        return true;
                    FramesMax=Multiplier-1;
                }
                Frames=S;
                Flags.reset(DropFrame);
                Flags.reset(FramesPerSecond_Is1001);
                Flags.reset(MustUseSecondField);
                Flags.reset(IsSecondField);
                Flags.reset(IsNegative);
                Flags.reset(HasNoFramesInfo);
                Flags.set(IsTime);
            }
            //hh:mm:ss;ff format
            else if (Length==11
             && (Value[8]==':' || Value[8]==';')
             && Value[9]>='0' && Value[9]<='9'
             && Value[10]>='0' && Value[10]<='9')
            {
                Frames=((Value[9]-'0')*10)+(Value[10]-'0');
                Flags.set(DropFrame, Value[8]==';');
                if (Value[8])
                    Flags.set(FramesPerSecond_Is1001);
                Flags.reset(IsSecondField);
                Flags.reset(IsNegative);
                Flags.reset(HasNoFramesInfo);
                Flags.reset(IsTime);
            }
            else
            {
                *this=TimeCode();
                return true;
            }
        }
        else
        {
            Frames=0;
            FramesMax=0;
            Flags.reset(IsSecondField);
            Flags.reset(IsNegative);
            Flags.set(HasNoFramesInfo);
            Flags.reset(IsTime);
        }
        Hours=((Value[0]-'0')*10)+(Value[1]-'0');
        Minutes=((Value[3]-'0')*10)+(Value[4]-'0');
        Seconds=((Value[6]-'0')*10)+(Value[7]-'0');
        Flags.set(IsValid);
        return false;
    }

    //Get unit
    if (!Length)
    {
        *this=TimeCode();
        return true;
    }
    char Unit=Value[Length-1];
    Length--; //Remove the unit from the string

    switch (Unit)
    {
        //X.X format based on time
        case 's':
        case 'm':
        case 'h':
            {
            if (Unit=='s' && Length && Value[Length-1]=='m')
            {
                Length--; //Remove the unit from the string
                Unit='n'; //Magic value for ms
            }
            unsigned char c;
            int i=0;
            int32_t S=0;
            int TheoriticalMax=i+PowersOf10_Size;
            int MaxLength=Length>TheoriticalMax?TheoriticalMax:Length;
            while (i<MaxLength)
            {
                c=(unsigned char)Value[i];
                c-='0';
                if (c>9)
                    break;
                S*=10;
                S+=c;
                i++;
            }
            switch (Unit)
            {
                case 'n':
                    Hours=S/3600000;
                    Minutes=(S%3600000)/60000;
                    Seconds=(S%60000)/1000;
                    S%=1000;
                    break;
                case 's':
                    Hours=S/3600;
                    Minutes=(S%3600)/60;
                    Seconds=S%60;
                    break;
                case 'm':
                    Hours=S/60;
                    Minutes=S%60;
                    break;
                case 'h':
                    Hours=S;
                    break;
            }
            Flags.reset();
            Flags.set(IsTime);
            Flags.set(IsValid);
            c=(unsigned char)Value[i];
            if (c=='.' || c==',')
            {
                i++;
                if (i==Length)
                    return true;
                size_t i_Start=i;
                uint64_t T=0;
                TheoriticalMax=i+PowersOf10_Size;
                MaxLength=Length>TheoriticalMax?TheoriticalMax:Length;
                while (i<MaxLength)
                {
                    c=(unsigned char)Value[i];
                    c-='0';
                    if (c>9)
                        break;
                    T*=10;
                    T+=c;
                    i++;
                }
                if (i!=Length)
                    return true;
                int FramesRate_Index=i-1-i_Start;
                uint64_t FramesRate=PowersOf10[FramesRate_Index];
                FramesMax=(uint32_t)(FramesRate-1);
                switch (Unit)
                {
                    case 'h':
                    {
                        T*=3600;
                        uint64_t T_Divider=PowersOf10[2];
                        T=(T+T_Divider/2)/T_Divider;
                        FramesRate/=T_Divider;
                        uint64_t Temp2=T/FramesRate;
                        Minutes=Temp2/60;
                        if (Minutes>=60)
                        {
                            Minutes=0;
                            Hours++;
                        }
                        Seconds=Temp2%60;
                        T%=FramesRate;
                        FramesMax=FramesRate-1;
                        break;
                    }
                    case 'm':
                    {
                        T*=60;
                        uint64_t T_Divider=PowersOf10[0];
                        T=(T+T_Divider/2)/T_Divider;
                        FramesRate/=T_Divider;
                        Seconds=T/FramesRate;
                        if (Seconds>=60)
                        {
                            Seconds=0;
                            Minutes++;
                            if (Minutes>=60)
                            {
                                Hours++;
                                Minutes=0;
                            }
                        }
                        T%=FramesRate;
                        FramesMax=FramesRate-1;
                        break;
                    }
                    case 'n':
                    {
                        FramesRate*=1000;
                        T+=((uint64_t)S)*FramesRate;
                        uint64_t T_Divider=PowersOf10[1];
                        if (FramesRate_Index>5)
                        {
                            uint64_t T_Divider=PowersOf10[8-FramesRate_Index];
                            T=(T+T_Divider/2)/T_Divider;
                            FramesRate=PowersOf10[8];
                        }
                        FramesMax=(uint32_t)(FramesRate-1);
                        break;
                    }
                }
                Frames=T;
            }
            else if (Unit=='n')
            {
                Frames=S;
                FramesMax=1000;
            }
            else
            {
                Frames=0;
                FramesMax=0;
            }
            Flags.set(IsValid);
            return false;
            }
            break;

        //X format based on rate
        case 'f':
        case 't':
            {
            unsigned char c;
            int i=0;
            int32_t S=0;
            int TheoriticalMax=i+PowersOf10_Size;
            int MaxLength=Length>TheoriticalMax?TheoriticalMax:Length;
            while (i<MaxLength)
            {
                c=(unsigned char)Value[i];
                c-='0';
                if (c>9)
                    break;
                S*=10;
                S+=c;
                i++;
            }
            if (i!=Length)
                return true;
            uint32_t FrameRate=(uint32_t)FramesMax+1;
            int32_t OneHourInFrames=3600*FrameRate;
            int32_t OneMinuteInFrames=60*FrameRate;
            Hours=S/OneHourInFrames;
            Minutes=(S%OneHourInFrames)/OneMinuteInFrames;
            Seconds=(S%OneMinuteInFrames)/FrameRate;
            Frames=S%FrameRate;
            Flags.reset(MustUseSecondField);
            Flags.reset(IsSecondField);
            Flags.reset(IsNegative);
            Flags.reset(HasNoFramesInfo);
            Flags.set(IsTime, Unit=='t');
            Flags.set(IsValid);
            return false;
            }
            break;
    }

    *this=TimeCode();
    return true;
}

//---------------------------------------------------------------------------
string TimeCode::ToString() const
{
    if (!HasValue())
        return string();
    string TC;
    if (Flags.test(IsNegative))
        TC+='-';
    uint8_t HH=Hours;
    if (HH>100)
    {
        TC+=to_string(HH/100);
        HH%=100;
    }
    TC+=('0'+HH/10);
    TC+=('0'+HH%10);
    TC+=':';
    uint8_t MM=Minutes;
    if (MM>100)
    {
        TC+=('0'+MM/100);
        MM%=100;
    }
    TC+=('0'+MM/10);
    TC+=('0'+MM%10);
    TC+=':';
    uint8_t SS=Seconds;
    if (SS>100)
    {
        TC+=('0'+SS/100);
        SS%=100;
    }
    TC+=('0'+SS/10);
    TC+=('0'+SS%10);
    bool d=Flags.test(DropFrame);
    bool t=Flags.test(IsTime);
    if (!t && d)
        TC+=';';
    if (t)
    {
        int AfterCommaMinus1;
        AfterCommaMinus1=PowersOf10_Size;
        auto FrameRate=FramesMax+1;
        while ((--AfterCommaMinus1)>=0 && PowersOf10[AfterCommaMinus1]!=FrameRate);
        TC+='.';
        if (AfterCommaMinus1<0)
        {
            stringstream s;
            s<<Frames;
            TC+=s.str();
            TC+='S';
            s.str(string());
            s<<FrameRate;
            TC+=s.str();
        }
        else
        {
            for (int i=0; i<=AfterCommaMinus1;i++)
                TC+='0'+(Frames/(i==AfterCommaMinus1?1:PowersOf10[AfterCommaMinus1-i-1])%10);
        }
    }
    else if (!Flags.test(HasNoFramesInfo))
    {
        if (!d)
            TC+=':';
        auto FF=Frames;
        if (FF>=100)
        {
            TC+=to_string(FF/100);
            FF%=100;
        }
        TC+=('0'+(FF/10));
        TC+=('0'+(FF%10));
        if (Flags.test(MustUseSecondField) || Flags.test(IsSecondField))
        {
            TC+='.';
            TC+=('0'+Flags.test(IsSecondField));
        }
    }

    return TC;
}

//---------------------------------------------------------------------------
int64_t TimeCode::ToFrames() const
{
    if (!HasValue())
        return 0;

    int64_t TC=(int64_t(Hours)     *3600
             + int64_t(Minutes)   *  60
             + int64_t(Seconds)        )*(FramesMax+1);

    if (Flags.test(DropFrame) && FramesMax)
    {
        uint64_t Dropped=FramesMax/30+1;

        TC-= int64_t(Hours)      *108*Dropped
          + (int64_t(Minutes)/10)*18*Dropped
          + (int64_t(Minutes)%10)* 2*Dropped;
    }

    if (!Flags.test(HasNoFramesInfo) && FramesMax)
        TC+=Frames;
    if (Flags.test(MustUseSecondField))
        TC<<=1;
    if (Flags.test(IsSecondField))
        TC++;
    if (Flags.test(IsNegative))
        TC=-TC;

    return TC;
}

//---------------------------------------------------------------------------
int64_t TimeCode::ToMilliseconds() const
{
    if (!HasValue())
        return 0;

    int64_t Den=((((uint64_t)FramesMax)+1)*(Flags.test(MustUseSecondField)?2:1));
    int64_t MS=(ToFrames()*1000*(FramesMax && (Flags.test(DropFrame) || Flags.test(FramesPerSecond_Is1001))?1.001:1.000)+Den/2)/Den;

    if (Flags.test(IsNegative))
        MS=-MS;

    return MS;
}
