/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* ************************** */
/* Include Header File */
/* ************************** */
#define OISCMD

/* #include      "Main.h" */
/* #include      "Cmd.h" */
#include "Ois.h"
#include "OisDef.h"

struct stAdjPar StAdjPar;  /* Execute Command Parameter */
unsigned char UcOscAdjFlg; /* For Measure trigger */
unsigned long UlH1Coefval; /* H1 coefficient value */
unsigned char UcH1LvlMod;  /* H1 level coef mode */

/* ************************** */
/* define */
/* ************************** */
#define MES_XG1 0 /* LXG1 Measure Mode */
#define MES_XG2 1 /* LXG2 Measure Mode */

#define HALL_ADJ 0
#define LOOPGAIN 1
#define THROUGH 2
#define NOISE 3

/* Measure Mode */

#define TNE 80 /* Waiting Time For Movement */
#ifdef HALLADJ_HW

#define __MEASURE_LOOPGAIN 0x00
#define __MEASURE_BIASOFFSET 0x01

#else

/******* Hall calibration Type 1 *******/
#define MARJIN 0x0300 /* Marjin */
#define BIAS_ADJ_BORDER                                                        \
	0x1998 /* HALL_MAX_GAP < BIAS_ADJ_BORDER < HALL_MIN_GAP(80%) */

#define HALL_MAX_GAP (BIAS_ADJ_BORDER - MARJIN)
#define HALL_MIN_GAP (BIAS_ADJ_BORDER + MARJIN)


#define BIAS_LIMIT 0xFFFF /* HALL BIAS LIMIT */
#define OFFSET_DIV 2      /* Divide Difference For Offset Step */
#define TIME_OUT 40 /* Time Out Count */

/******* Hall calibration Type 2 *******/
#define MARGIN 0x0300 /* Margin */

#define BIAS_ADJ_OVER 0xD998  /* 85% */
#define BIAS_ADJ_RANGE 0xCCCC /* 80% */
#define BIAS_ADJ_SKIP 0xBFFF  /* 75% */
#define HALL_MAX_RANGE (BIAS_ADJ_RANGE + MARGIN)
#define HALL_MIN_RANGE (BIAS_ADJ_RANGE - MARGIN)

#define DECRE_CAL 0x0100 /* decrease value */

#endif

#ifdef H1COEF_CHANGER
#ifdef CORRECT_1DEG
#define MAXLMT 0x40400000  /* 3.0 */
#define MINLMT 0x3FE66666  /* 1.8 */
#define CHGCOEF 0xBA195555 /*  */
#else
#define MAXLMT 0x40000000  /* 2.0 */
#define MINLMT 0x3F8CCCCD  /* 1.1 */
#define CHGCOEF 0xBA4C71C7 /*  */
#endif
#define MINLMT_MOV 0x00000000 /* 0.0 */
#define CHGCOEF_MOV 0xB9700000
#endif

/* ************************** */
/* Global Variable */
/* ************************** */
#ifdef HALLADJ_HW
unsigned char UcAdjBsy;

#else
unsigned short UsStpSiz; /* Bias Step Size */
unsigned short UsErrBia, UsErrOfs;
#endif

/* ************************** */
/* Const */
/* ************************** */
/* gxzoom Setting Value */
#define ZOOMTBL 16
const unsigned long ClGyxZom[ZOOMTBL] = {
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};

/* gyzoom Setting Value */
const unsigned long ClGyyZom[ZOOMTBL] = {
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
	0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000};

/* DI Coefficient Setting Value */
#define COEFTBL 7
const unsigned long ClDiCof[COEFTBL] = {
	DIFIL_S2, /* 0 */
	DIFIL_S2, /* 1 */
	DIFIL_S2, /* 2 */
	DIFIL_S2, /* 3 */
	DIFIL_S2, /* 4 */
	DIFIL_S2, /* 5 */
	DIFIL_S2  /* 6 */
};


#ifndef HALLADJ_HW


unsigned short UsValBef, UsValNow;

#endif

void SrvCon(unsigned char UcDirSel, unsigned char UcSwcCon)
{
	if (UcSwcCon) {
		if (!UcDirSel) {			      /* X Direction */
			RegWriteA_LC898122AF(WH_EQSWX, 0x03); /* 0x0170 */
			RamWrite32A_LC898122AF(sxggf, 0x00000000); /* 0x10B5 */
		} else {				      /* Y Direction */
			RegWriteA_LC898122AF(WH_EQSWY, 0x03); /* 0x0171 */
			RamWrite32A_LC898122AF(syggf, 0x00000000); /* 0x11B5 */
		}
	} else {
		if (!UcDirSel) {			      /* X Direction */
			RegWriteA_LC898122AF(WH_EQSWX, 0x02); /* 0x0170 */
			RamWrite32A_LC898122AF(SXLMT, 0x00000000); /* 0x1477 */
		} else {				      /* Y Direction */
			RegWriteA_LC898122AF(WH_EQSWY, 0x02); /* 0x0171 */
			RamWrite32A_LC898122AF(SYLMT, 0x00000000); /* 0x14F7 */
		}
	}
}

#ifdef MODULE_CALIBRATION

unsigned char LopGan(unsigned char UcDirSel)
{
	unsigned char UcLpAdjSts;

#ifdef HALLADJ_HW
	UcLpAdjSts = LoopGainAdj(UcDirSel);
#else
	MesFil(LOOPGAIN);

	/* Servo ON */
	SrvCon(X_DIR, ON);
	SrvCon(Y_DIR, ON);

	/* Wait 300ms */
	WitTim_LC898122AF(300);

	/* Loop Gain Adjust Initialize */
	LopIni(UcDirSel);

	/* Loop Gain Adjust */
	UcLpAdjSts = LopAdj(UcDirSel);
#endif
	/* Servo OFF */
	SrvCon(X_DIR, OFF);
	SrvCon(Y_DIR, OFF);

	if (!UcLpAdjSts)
		return EXE_END;

	if (!UcDirSel)
		return EXE_LXADJ;
	else
		return EXE_LYADJ;
}

#ifndef HALLADJ_HW

void LopIni(unsigned char UcDirSel)
{
	/* Loop Gain Value Initialize */
	LopPar(UcDirSel);

	/* Sign Wave Output Setting */
	LopSin(UcDirSel, ON);
}
#endif


void LopPar(unsigned char UcDirSel)
{
	unsigned short UsLopGan;

	RamAccFixMod(ON); /* Fix mode */

	if (!UcDirSel) {
		UsLopGan = SXGAIN_LOP;
		RamWriteA_LC898122AF(sxg, UsLopGan); /* 0x10D3 */
	} else {
		UsLopGan = SYGAIN_LOP;
		RamWriteA_LC898122AF(syg, UsLopGan); /* 0x11D3 */
	}

	RamAccFixMod(OFF); /* Float mode */
}

#ifndef HALLADJ_HW

void LopSin(unsigned char UcDirSel, unsigned char UcSonOff)
{
	unsigned short UsFreqVal;
	unsigned char UcEqSwX, UcEqSwY;

	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */

	if (UcSonOff) {

#ifdef USE_EXTCLK_ALL /* 24MHz */
		      /* Freq = CmSinFrq * 11.718kHz / 65536 / 16 */
#ifdef ACTREG_6P5OHM
		/* UsFreqVal       =       0x30EE ;  */ /* 139.9Hz */
		UsFreqVal = 0x29F1;			/* 119.9Hz */
#endif
#ifdef ACTREG_10P2OHM
		UsFreqVal = 0x29F1; /* 119.9Hz */
#endif
#ifdef ACTREG_15OHM
		UsFreqVal = 0x3B6B; /* 169.9Hz */
#endif
#else
/* Freq = CmSinFrq * 23.4375kHz / 65536 / 16 */
#ifdef ACTREG_6P5OHM
		/* UsFreqVal       =       0x1877 ;   */ /* 139.9Hz */
		UsFreqVal = 0x14F8; /* 119.9Hz */
#endif
#ifdef ACTREG_10P2OHM
		UsFreqVal = 0x14F8; /* 119.9Hz */
#endif
#ifdef ACTREG_15OHM
		UsFreqVal = 0x1DB5; /* 169.9Hz */
#endif
#endif

		RegWriteA_LC898122AF(WC_SINFRQ0,
				     (unsigned char)
					     UsFreqVal); /* 0x0181 Freq L */
		RegWriteA_LC898122AF(WC_SINFRQ1,
				     (unsigned char)(UsFreqVal >> 8));

		if (!UcDirSel) {

			UcEqSwX |= 0x10;
			UcEqSwY &= ~EQSINSW;

			RamWrite32A_LC898122AF(sxsin,
					       0x3CA3D70A); /* 0x10D5 -34dB */
		} else {

			UcEqSwX &= ~EQSINSW;
			UcEqSwY |= 0x10;

			RamWrite32A_LC898122AF(sysin,
					       0x3CA3D70A); /* 0x11D5 -34dB */
		}
		RegWriteA_LC898122AF(WC_SINPHSX,
				     0x00); /* 0x0183       X Sine phase */
		RegWriteA_LC898122AF(WC_SINPHSY,
				     0x00); /* 0x0184       Y Sine phase */
		RegWriteA_LC898122AF(WH_EQSWX,
				     UcEqSwX); /* 0x0170       Switch control */
		RegWriteA_LC898122AF(WH_EQSWY,
				     UcEqSwY); /* 0x0171       Switch control */
		RegWriteA_LC898122AF(WC_SINON,
				     0x01); /* 0x0180       Sine wave  */
	} else {
		UcEqSwX &= ~EQSINSW;
		UcEqSwY &= ~EQSINSW;
		RegWriteA_LC898122AF(WC_SINON,
				     0x00); /* 0x0180       Sine wave  */
		if (!UcDirSel)
			RamWrite32A_LC898122AF(sxsin, 0x00000000); /* 0x10D5 */
		else
			RamWrite32A_LC898122AF(sysin, 0x00000000); /* 0x11D5 */

		RegWriteA_LC898122AF(WH_EQSWX,
				     UcEqSwX); /* 0x0170       Switch control */
		RegWriteA_LC898122AF(WH_EQSWY,
				     UcEqSwY); /* 0x0171       Switch control */
	}
}


unsigned char LopAdj(unsigned char UcDirSel)
{
	unsigned char UcAdjSts = FAILURE;
	unsigned short UsRtnVal;
	float SfCmpVal;
	unsigned char UcIdxCnt;
	unsigned char UcIdxCn1;
	unsigned char UcIdxCn2;
	union UnFltVal UnAdcXg1, UnAdcXg2, UnRtnVa;

	float DfGanVal[5];
	float DfTemVal;

	if (!UcDirSel) {
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)SXGZ); /* 0x0194 */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((SXGZ >> 8) & 0x0001)); /* 0x0195 */
		RegWriteA_LC898122AF(WC_MES2ADD0,
				     (unsigned char)SXG3Z); /* 0x0196 */
		RegWriteA_LC898122AF(
			WC_MES2ADD1,
			(unsigned char)((SXG3Z >> 8) & 0x0001)); /* 0x0197 */
	} else {
		RegWriteA_LC898122AF(WC_MES1ADD0,
				     (unsigned char)SYGZ); /* 0x0194 */
		RegWriteA_LC898122AF(
			WC_MES1ADD1,
			(unsigned char)((SYGZ >> 8) & 0x0001)); /* 0x0195 */
		RegWriteA_LC898122AF(WC_MES2ADD0,
				     (unsigned char)SYG3Z); /* 0x0196 */
		RegWriteA_LC898122AF(
			WC_MES2ADD1,
			(unsigned char)((SYG3Z >> 8) & 0x0001)); /* 0x0197 */
	}

	/* 5 Times Average Value Calculation */
	for (UcIdxCnt = 0; UcIdxCnt < 5; UcIdxCnt++) {
		LopMes(); /* Loop Gain Mesurement Start */

		UnAdcXg1.UlLngVal = GinMes(MES_XG1); /* LXG1 Measure */
		UnAdcXg2.UlLngVal = GinMes(MES_XG2); /* LXG2 Measure */

		SfCmpVal = UnAdcXg2.SfFltVal /
			   UnAdcXg1.SfFltVal; /* Compare Coefficient Value */

		if (!UcDirSel)
			RamRead32A_LC898122AF(sxg,
					      &UnRtnVa.UlLngVal); /* 0x10D3 */
		else
			RamRead32A_LC898122AF(syg,
					      &UnRtnVa.UlLngVal); /* 0x11D3 */

		UnRtnVa.SfFltVal = UnRtnVa.SfFltVal * SfCmpVal;

		DfGanVal[UcIdxCnt] = UnRtnVa.SfFltVal;
	}

	for (UcIdxCn1 = 0; UcIdxCn1 < 4; UcIdxCn1++) {
		for (UcIdxCn2 = UcIdxCn1 + 1; UcIdxCn2 < 5; UcIdxCn2++) {
			if (DfGanVal[UcIdxCn1] > DfGanVal[UcIdxCn2]) {
				DfTemVal = DfGanVal[UcIdxCn1];
				DfGanVal[UcIdxCn1] = DfGanVal[UcIdxCn2];
				DfGanVal[UcIdxCn2] = DfTemVal;
			}
		}
	}

	UnRtnVa.SfFltVal = (DfGanVal[1] + DfGanVal[2] + DfGanVal[3]) / 3;

	LopSin(UcDirSel, OFF);

	if (UnRtnVa.UlLngVal < 0x3F800000) /* Adjust Error */
		UcAdjSts = SUCCESS;	/* Status OK */

	if (UcAdjSts) {
		if (!UcDirSel) {
			RamWrite32A_LC898122AF(sxg, 0x3F800000); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = 0x7FFF;
			StAdjPar.StLopGan.UsLxgSts = 0x0000;
		} else {
			RamWrite32A_LC898122AF(syg, 0x3F800000); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = 0x7FFF;
			StAdjPar.StLopGan.UsLygSts = 0x0000;
		}
	} else {
		if (!UcDirSel) {
			RamWrite32A_LC898122AF(sxg,
					       UnRtnVa.UlLngVal); /* 0x10D3 */
			RamAccFixMod(ON);			  /* Fix mode */
			RamReadA_LC898122AF(sxg, &UsRtnVal);      /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRtnVal;
			StAdjPar.StLopGan.UsLxgSts = 0xFFFF;
		} else {
			RamWrite32A_LC898122AF(syg,
					       UnRtnVa.UlLngVal); /* 0x11D3 */
			RamAccFixMod(ON);			  /* Fix mode */
			RamReadA_LC898122AF(syg, &UsRtnVal);      /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRtnVal;
			StAdjPar.StLopGan.UsLygSts = 0xFFFF;
		}
		RamAccFixMod(OFF); /* Float mode */
	}
	return UcAdjSts;
}

void LopMes(void)
{
	ClrGyr(0x1000, CLR_FRAM1); /* Measure Filter RAM Clear */
	RamWrite32A_LC898122AF(MSABS1AV, 0x00000000); /* 0x1041       Clear */
	RamWrite32A_LC898122AF(MSABS2AV, 0x00000000); /* 0x1141       Clear */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x04);      /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x00); /* 0x0192       1024 Times Measure */
	RamWrite32A_LC898122AF(msmean,
			       0x3A800000); /* 0x1230       1/CmMesLoop[15:0] */
	RegWriteA_LC898122AF(WC_MESABS, 0x01);  /* 0x0198       ABS */
	RegWriteA_LC898122AF(WC_MESWAIT, 0x00); /* 0x0199       0 cross wait */
	BsyWit(WC_MESMODE, 0x01); /* 0x0190       Sin Wave Measure */
}


unsigned long GinMes(unsigned char UcXg1Xg2)
{
	unsigned long UlMesVal;

	if (!UcXg1Xg2)
		RamRead32A_LC898122AF(MSABS1AV, &UlMesVal); /* 0x1041 */
	else
		RamRead32A_LC898122AF(MSABS2AV, &UlMesVal); /* 0x1141 */

	return UlMesVal;
}

#endif
#endif


#define LIMITH 0x0FA0
#define LIMITL 0xF060
#define INITVAL 0x0000

unsigned char RtnCen(unsigned char UcCmdPar)
{
	unsigned char UcCmdSts;

	UcCmdSts = EXE_END;

	GyrCon(OFF); /* Gyro OFF */

	if (!UcCmdPar) { /* X,Y Centering */

		StbOnn(); /* Slope Mode */

	} else if (UcCmdPar == 0x01) { /* X Centering Only */

		SrvCon(X_DIR, ON); /* X only Servo ON */
		SrvCon(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) { /* Y Centering Only */

		SrvCon(X_DIR, OFF); /* Y only Servo ON */
		SrvCon(Y_DIR, ON);
	}

	return UcCmdSts;
}

void GyrCon(unsigned char UcGyrCon)
{
	/* Return HPF Setting */
	RegWriteA_LC898122AF(WG_SHTON, 0x00); /* 0x0107 */

	if (UcGyrCon == ON) { /* Gyro ON */

#ifdef GAIN_CONT
	/* Gain3 Register */
	/* AutoGainControlSw( ON ) ;  */ /* Auto Gain Control Mode ON */
#endif
		ClrGyr(0x000E, CLR_FRAM1); /* Gyro Delay RAM Clear */

		RamWrite32A_LC898122AF(sxggf, 0x3F800000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x3F800000); /* 0x11B5 */

	} else if (UcGyrCon == SPC) { /* Gyro ON for LINE */

#ifdef GAIN_CONT
	/* Gain3 Register */
	/* AutoGainControlSw( ON ) ;  */ /* Auto Gain Control Mode ON */
#endif

		RamWrite32A_LC898122AF(sxggf, 0x3F800000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x3F800000); /* 0x11B5 */

	} else { /* Gyro OFF */

		RamWrite32A_LC898122AF(sxggf, 0x00000000); /* 0x10B5 */
		RamWrite32A_LC898122AF(syggf, 0x00000000); /* 0x11B5 */

#ifdef GAIN_CONT
		/* Gain3 Register */
		/* AutoGainControlSw( OFF ); */
#endif
	}
}

void OisEna(void)
{
	/* Servo ON */
	SrvCon(X_DIR, ON);
	SrvCon(Y_DIR, ON);

	GyrCon(ON);
}




#ifdef USE_EXTCLK_ALL /* 24MHz */
		      /********* Parameter Setting *********/
/* Servo Sampling Clock         =       11.71875kHz */
/* Freq                                         =       CmSinFreq*Fs/65536/16 */
/* 05 00 XX MM                          XX:Freq MM:Sin or Circle */
const unsigned short CucFreqVal[17] = {
	0xFFFF, /* 0:  Stop */
	0x0059, /* 1: 0.994653Hz */
	0x00B2, /* 2: 1.989305Hz */
	0x010C, /* 3: 2.995133Hz */
	0x0165, /* 4: 3.989786Hz */
	0x01BF, /* 5: 4.995614Hz */
	0x0218, /* 6: 5.990267Hz */
	0x0272, /* 7: 6.996095Hz */
	0x02CB, /* 8: 7.990748Hz */
	0x0325, /* 9: 8.996576Hz */
	0x037E, /* A: 9.991229Hz */
	0x03D8, /* B: 10.99706Hz */
	0x0431, /* C: 11.99171Hz */
	0x048B, /* D: 12.99754Hz */
	0x04E4, /* E: 13.99219Hz */
	0x053E, /* F: 14.99802Hz */
	0x0597  /* 10: 15.99267Hz */
};
#else
/********* Parameter Setting *********/
/* Servo Sampling Clock         =       23.4375kHz */
/* Freq                                         =       CmSinFreq*Fs/65536/16 */
/* 05 00 XX MM                          XX:Freq MM:Sin or Circle */
const unsigned short CucFreqVal[17] = {
	0xFFFF, /* 0:  Stop */
	0x002C, /* 1: 0.983477Hz */
	0x0059, /* 2: 1.989305Hz */
	0x0086, /* 3: 2.995133Hz */
	0x00B2, /* 4: 3.97861Hz */
	0x00DF, /* 5: 4.984438Hz */
	0x010C, /* 6: 5.990267Hz */
	0x0139, /* 7: 6.996095Hz */
	0x0165, /* 8: 7.979572Hz */
	0x0192, /* 9: 8.9854Hz */
	0x01BF, /* A: 9.991229Hz */
	0x01EC, /* B: 10.99706Hz */
	0x0218, /* C: 11.98053Hz */
	0x0245, /* D: 12.98636Hz */
	0x0272, /* E: 13.99219Hz */
	0x029F, /* F: 14.99802Hz */
	0x02CB  /* 10: 15.9815Hz */
};
#endif

#define USE_SINLPF

#ifdef STANDBY_MODE
#endif

void SetZsp(unsigned char UcZoomStepDat)
{
	unsigned long UlGyrZmx, UlGyrZmy, UlGyrZrx, UlGyrZry;

	/* Zoom Step */
	if (UcZoomStepDat > (ZOOMTBL - 1))
		UcZoomStepDat = (ZOOMTBL - 1); /* è„å¿ÇZOOMTBL-1Ç…ê›íËÇ∑ÇÈ */

	if (UcZoomStepDat == 0) {       /* initial setting        */
		UlGyrZmx = ClGyxZom[0]; /* Same Wide Coefficient */
		UlGyrZmy = ClGyyZom[0]; /* Same Wide Coefficient */
					/* Initial Rate value = 1 */
	} else {
		UlGyrZmx = ClGyxZom[UcZoomStepDat];
		UlGyrZmy = ClGyyZom[UcZoomStepDat];
	}

	/* Zoom Value Setting */
	RamWrite32A_LC898122AF(gxlens, UlGyrZmx); /* 0x1022 */
	RamWrite32A_LC898122AF(gylens, UlGyrZmy); /* 0x1122 */

	RamRead32A_LC898122AF(gxlens, &UlGyrZrx); /* 0x1022 */
	RamRead32A_LC898122AF(gylens, &UlGyrZry); /* 0x1122 */

	/* Zoom Value Setting Error Check */
	if (UlGyrZmx != UlGyrZrx)
		RamWrite32A_LC898122AF(gxlens, UlGyrZmx); /* 0x1022 */

	if (UlGyrZmy != UlGyrZry)
		RamWrite32A_LC898122AF(gylens, UlGyrZmy); /* 0x1122 */
}

void StbOnn(void)
{
	unsigned char UcRegValx, UcRegValy; /* Registor value */
	unsigned char UcRegIni;

	RegReadA_LC898122AF(WH_EQSWX, &UcRegValx); /* 0x0170 */
	RegReadA_LC898122AF(WH_EQSWY, &UcRegValy); /* 0x0171 */

	if (((UcRegValx & 0x01) != 0x01) && ((UcRegValy & 0x01) != 0x01)) {

		RegWriteA_LC898122AF(WH_SMTSRVON, 0x01);

		SrvCon(X_DIR, ON);
		SrvCon(Y_DIR, ON);

		UcRegIni = 0x11;
		while ((UcRegIni & 0x77) != 0x66)
			RegReadA_LC898122AF(RH_SMTSRVSTT, &UcRegIni);

		RegWriteA_LC898122AF(WH_SMTSRVON, 0x00);

	} else {
		SrvCon(X_DIR, ON);
		SrvCon(Y_DIR, ON);
	}
}


#ifdef MODULE_CALIBRATION

#define RRATETABLE 8
#define CRATETABLE 16
const signed char ScRselRate[RRATETABLE] = {
	-12, /* -12% */
	-9,  /*  -9% */
	-6,  /*  -6% */
	-3,  /*  -3% */
	0,   /*   0% */
	3,   /*   3% */
	7,   /*   7% */
	11   /*  11% */
};

const signed char ScCselRate[CRATETABLE] = {
	-14, /* -14% */
	-12, /* -12% */
	-10, /* -10% */
	-8,  /*  -8% */
	-6,  /*  -6% */
	-4,  /*  -4% */
	-2,  /*  -2% */
	0,   /*   0% */
	0,   /*   0% */
	2,   /*   2% */
	4,   /*   4% */
	6,   /*   6% */
	8,   /*   8% */
	10,  /*  10% */
	12,  /*  12% */
	14   /*  14% */
};

#define TARGET_FREQ 48000.0F /* 48MHz */

#define START_RSEL 0x04 /* Typ */
#define START_CSEL 0x08 /* Typ bit4:OSCPMSEL */
#define MEAS_MAX 32     /* è„å¿32âÒ */
/* Measure Status (UcClkJdg) */
#define UNDR_MEAS 0x00
#define FIX_MEAS 0x01
#define JST_FIX 0x03
#define OVR_MEAS 0x80
/* Measure Check Flag (UcMeasFlg) */
#define RSELFX 0x08
#define RSEL1ST 0x01
#define RSEL2ND 0x02
#define CSELFX 0x80
#define CSELPLS 0x10
#define CSELMNS 0x20

unsigned short OscAdj(void)
{
	unsigned char UcMeasFlg;	    /* Measure check flag */
	union UnWrdVal StClkVal;	    /* Measure value */
	unsigned char UcMeasCnt;	    /* Measure counter */
	unsigned char UcOscrsel, UcOsccsel; /* Reg set value */
	unsigned char UcSrvDivBk;	   /* back up value */
	unsigned char UcClkJdg;		    /* State flag */
	float FcalA, FcalB;		    /* calcurate value */
	signed char ScTblRate_Val, ScTblRate_Now, ScTblRate_Tgt;
	float FlRatePbk, FlRateMbk;
	unsigned char UcOsccselP, UcOsccselM; /* Reg set value */
	unsigned short UsResult;

	/* unsigned char   UcOscsetBk ;  */ /* Reg set value */

	UcMeasFlg = 0;	/* Clear Measure check flag */
	UcMeasCnt = 0;	/* Clear Measure counter */
	UcClkJdg = UNDR_MEAS; /* under Measure */
	UcOscrsel = START_RSEL;
	UcOsccsel = START_CSEL;
	/* check */
	/* RegReadA_LC898122AF( OSCSET, &UcOscsetBk ) ;       // 0x0264 */
	/* UcOscrsel = ( UcOscsetBk & 0xE0 ) >> 5 ; */
	/* UcOsccsel = ( UcOscsetBk & 0x1E ) >> 1 ; */
	/**/ RegReadA_LC898122AF(SRVDIV, &UcSrvDivBk); /* 0x0211 */
	RegWriteA_LC898122AF(SRVDIV,
			     0x00); /* 0x0211        SRV Clock = Xtalck */
	RegWriteA_LC898122AF(OSCSET,
			     (UcOscrsel << 5) | (UcOsccsel << 1)); /* 0x0257 */

	while (UcClkJdg == UNDR_MEAS) {
		UcMeasCnt++;	   /* Measure count up */
		UcOscAdjFlg = MEASSTR; /* Start trigger ON */

		while (UcOscAdjFlg != MEASFIX)
			;

		UcOscAdjFlg = 0x00; /* Clear Flag */
		RegReadA_LC898122AF(OSCCK_CNTR0,
				    &StClkVal.StWrdVal.UcLowVal); /* 0x025E */
		RegReadA_LC898122AF(OSCCK_CNTR1,
				    &StClkVal.StWrdVal.UcHigVal); /* 0x025F */

		FcalA = (float)StClkVal.UsWrdVal;
		FcalB = TARGET_FREQ / FcalA;
		FcalB = FcalB - 1.0F;
		FcalB *= 100.0F;

		if (FcalB == 0.0F) {
			UcClkJdg = JST_FIX;		/* Just 36MHz */
			UcMeasFlg |= (CSELFX | RSELFX); /* Fix Flag */
			break;
		}

		/* Rsel check */
		if (!(UcMeasFlg & RSELFX)) {
			if (UcMeasFlg & RSEL1ST)
				UcMeasFlg |= (RSELFX | RSEL2ND);
			else
				UcMeasFlg |= RSEL1ST;
			ScTblRate_Now = ScRselRate[UcOscrsel]; /* ç°ÇÃRate */
			ScTblRate_Tgt = ScTblRate_Now + (short)FcalB;
			if (ScTblRate_Now > ScTblRate_Tgt) {
				while (1) {
					if (UcOscrsel == 0)
						break;
					UcOscrsel -= 1;
					ScTblRate_Val = ScRselRate[UcOscrsel];
					if (ScTblRate_Tgt >= ScTblRate_Val)
						break;
				}
			} else if (ScTblRate_Now < ScTblRate_Tgt) {
				while (1) {
					if (UcOscrsel == (RRATETABLE - 1))
						break;
					UcOscrsel += 1;
					ScTblRate_Val = ScRselRate[UcOscrsel];
					if (ScTblRate_Tgt <= ScTblRate_Val)
						break;
				}
			} else {
				;
			}
		} else {
			/* Csel check */
			if (FcalB > 0) { /* Plus */
				UcMeasFlg |= CSELPLS;
				FlRatePbk = FcalB;
				UcOsccselP = UcOsccsel;
				if (UcMeasFlg & CSELMNS) {
					UcMeasFlg |= CSELFX;
					UcClkJdg = FIX_MEAS; /* OK */
				} else if (UcOsccsel == (CRATETABLE - 1)) {
					if (UcOscrsel < (RRATETABLE - 1)) {
						UcOscrsel += 1;
						UcOsccsel = START_CSEL;
						UcMeasFlg = 0; /* Clear */
					} else {
						UcClkJdg = OVR_MEAS; /* Over */
					}
				} else {
					UcOsccsel += 1;
				}
			} else { /* Minus */

				UcMeasFlg |= CSELMNS;
				FlRateMbk = (-1) * FcalB;
				UcOsccselM = UcOsccsel;
				if (UcMeasFlg & CSELPLS) {
					UcMeasFlg |= CSELFX;
					UcClkJdg = FIX_MEAS; /* OK */
				} else if (UcOsccsel == 0x00) {
					if (UcOscrsel > 0) {
						UcOscrsel -= 1;
						UcOsccsel = START_CSEL;
						UcMeasFlg = 0; /* Clear */
					} else {
						UcClkJdg = OVR_MEAS; /* Over */
					}
				} else {
					UcOsccsel -= 1;
				}
			}
			if (UcMeasCnt >= MEAS_MAX)
				UcClkJdg = OVR_MEAS; /* Over */
		}
		RegWriteA_LC898122AF(OSCSET,
				     (UcOscrsel << 5) |
					     (UcOsccsel << 1)); /* 0x0257 */
	}

	UsResult = EXE_END;

	if (UcClkJdg == FIX_MEAS) {
		if (FlRatePbk < FlRateMbk)
			UcOsccsel = UcOsccselP;
		else
			UcOsccsel = UcOsccselM;

		RegWriteA_LC898122AF(OSCSET,
				     (UcOscrsel << 5) |
					     (UcOsccsel << 1)); /* 0x0264 */

		/* check */
		/* RegReadA_LC898122AF( OSCSET, &UcOscsetBk ) ;       // 0x0257
		 */
	}
	StAdjPar.UcOscVal = ((UcOscrsel << 5) | (UcOsccsel << 1));

	if (UcClkJdg == OVR_MEAS) {
		UsResult = EXE_OCADJ;
		StAdjPar.UcOscVal = 0x00;
	}
	RegWriteA_LC898122AF(SRVDIV,
			     UcSrvDivBk); /* 0x0211        SRV Clock set */
	return UsResult;
}
#endif

#ifdef HALLADJ_HW

void SetSineWave(unsigned char UcJikuSel, unsigned char UcMeasMode)
{
#ifdef USE_EXTCLK_ALL						/* 24MHz */
	unsigned short UsFRQ[] = {0x30EE, 0x037E};
#else
	unsigned short UsFRQ[] = {0x1877, 0x01BF};
#endif
	unsigned long UlAMP[2][2] = {
		{0x3CA3D70A, 0x3CA3D70A},  /* Loop Gain   { X amp , Y amp } */
		{0x3F800000, 0x3F800000} }; /* Bias/offset { X amp , Y amp } */
	unsigned char UcEqSwX, UcEqSwY;

	UcMeasMode &= 0x01;
	UcJikuSel &= 0x01;

	/* Phase parameter 0deg */
	RegWriteA_LC898122AF(WC_SINPHSX, 0x00); /* 0x0183       */
	RegWriteA_LC898122AF(WC_SINPHSY, 0x00); /* 0x0184       */

	/* wait 0 cross */
	RegWriteA_LC898122AF(WC_MESSINMODE,
			     0x00);		/* 0x0191       Sine 0 cross  */
	RegWriteA_LC898122AF(WC_MESWAIT, 0x00); /* 0x0199       0 cross wait */

	/* Manually Set Amplitude */
	RamWrite32A_LC898122AF(sxsin, UlAMP[UcMeasMode][X_DIR]); /* 0x10D5 */
	RamWrite32A_LC898122AF(sysin, UlAMP[UcMeasMode][Y_DIR]); /* 0x11D5 */

	/* Freq */
	RegWriteA_LC898122AF(WC_SINFRQ0,
			     (unsigned char)
				     UsFRQ[UcMeasMode]); /* 0x0181 Freq L */
	RegWriteA_LC898122AF(WC_SINFRQ1,
			     (unsigned char)(UsFRQ[UcMeasMode] >> 8));

	/* Clear Optional Sine wave input address */
	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */
	if (!UcMeasMode && !UcJikuSel) {	 /* Loop Gain mode  X-axis */
		UcEqSwX |= 0x10;		 /* SW[4] */
		UcEqSwY &= ~EQSINSW;
	} else if (!UcMeasMode && UcJikuSel) { /* Loop Gain mode Y-Axis */
		UcEqSwX &= ~EQSINSW;
		UcEqSwY |= 0x10;	       /* SW[4] */
	} else if (UcMeasMode && !UcJikuSel) { /* Bias/Offset mode X-Axis */
		UcEqSwX = 0x22;		       /* SW[5] */
		UcEqSwY = 0x03;
	} else { /* Bias/Offset mode Y-Axis */
		UcEqSwX = 0x03;
		UcEqSwY = 0x22; /* SW[5] */
	}
	RegWriteA_LC898122AF(WH_EQSWX, UcEqSwX); /* 0x0170       */
	RegWriteA_LC898122AF(WH_EQSWY, UcEqSwY); /* 0x0171       */
}


void StartSineWave(void)
{
	/* Start Sine Wave */
	RegWriteA_LC898122AF(WC_SINON, 0x01); /* 0x0180       Sine wave  */
}


void StopSineWave(void)
{
	unsigned char UcEqSwX, UcEqSwY;

	RegWriteA_LC898122AF(WC_SINON, 0x00); /* 0x0180       Sine wave Stop */
	RegReadA_LC898122AF(WH_EQSWX, &UcEqSwX); /* 0x0170       */
	RegReadA_LC898122AF(WH_EQSWY, &UcEqSwY); /* 0x0171       */
	UcEqSwX &= ~EQSINSW;
	UcEqSwY &= ~EQSINSW;
	RegWriteA_LC898122AF(WH_EQSWX,
			     UcEqSwX); /* 0x0170       Switch control */
	RegWriteA_LC898122AF(WH_EQSWY,
			     UcEqSwY); /* 0x0171       Switch control */
}

void SetMeasFil(unsigned char UcFilSel)
{

	MesFil(UcFilSel); /* Set Measure filter */
}

void ClrMeasFil(void)
{
	/* Measure Filters clear */
	ClrGyr(0x1000, CLR_FRAM1); /* MEAS-FIL Delay RAM Clear */
}

#ifdef MODULE_CALIBRATION

unsigned char LoopGainAdj(unsigned char UcJikuSel)
{

	unsigned short UsRltVal;
	unsigned char UcAdjSts = FAILURE;

	UcJikuSel &= 0x01;

	StbOnn(); /* Slope Mode */

	/* Wait 200ms */
	WitTim_LC898122AF(200);

	/* set start gain */
	LopPar(UcJikuSel);

	/* set sine wave */
	SetSineWave(UcJikuSel, __MEASURE_LOOPGAIN);

	/* Measure count */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x00); /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x40); /* 0x0192       64 Times Measure */
	RamWrite32A_LC898122AF(msmean,
			       0x3C800000); /* 0x1230       1/CmMesLoop[15:0] */
	RegWriteA_LC898122AF(WC_MESABS, 0x01); /* 0x0198       ABS */

	/* Set Adjustment Limits */
	RamWrite32A_LC898122AF(
		LGMax,
		0x3F800000); /* 0x1092       Loop gain adjustment max limit */
	RamWrite32A_LC898122AF(
		LGMin,
		0x3E000100); /* 0x1091       Loop gain adjustment min limit */
	RegWriteA_LC898122AF(WC_AMJLOOP1,
			     0x00); /* 0x01A3       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJLOOP0,
			     0x41);		/* 0x01A2       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJIDL1, 0x00); /* 0x01A5       wait */
	RegWriteA_LC898122AF(WC_AMJIDL0, 0x00); /* 0x01A4       wait */

	/* set Measure Filter */
	SetMeasFil(LOOPGAIN);

	/* Clear Measure Filters */
	ClrMeasFil();

	/* Start Sine Wave */
	StartSineWave();

	/* Enable Loop Gain Adjustment */
	/* Check Busy Flag */
	BsyWit(WC_AMJMODE,
	       (0x0E |
		(UcJikuSel << 4))); /* 0x01A0       Loop Gain adjustment */

	RegReadA_LC898122AF(RC_AMJERROR, &UcAdjBsy); /* 0x01AD */

	/* Ram Access */
	RamAccFixMod(ON); /* Fix mode */

	if (UcAdjBsy) {
		if (UcJikuSel == X_DIR) {
			RamReadA_LC898122AF(sxg, &UsRltVal); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRltVal;
			StAdjPar.StLopGan.UsLxgSts = 0x0000;
		} else {
			RamReadA_LC898122AF(syg, &UsRltVal); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRltVal;
			StAdjPar.StLopGan.UsLygSts = 0x0000;
		}

	} else {
		if (UcJikuSel == X_DIR) {
			RamReadA_LC898122AF(sxg, &UsRltVal); /* 0x10D3 */
			StAdjPar.StLopGan.UsLxgVal = UsRltVal;
			StAdjPar.StLopGan.UsLxgSts = 0xFFFF;
		} else {
			RamReadA_LC898122AF(syg, &UsRltVal); /* 0x11D3 */
			StAdjPar.StLopGan.UsLygVal = UsRltVal;
			StAdjPar.StLopGan.UsLygSts = 0xFFFF;
		}
		UcAdjSts = SUCCESS; /* Status OK */
	}

	/* Ram Access */
	RamAccFixMod(OFF); /* Float mode */

	/* Stop Sine Wave */
	StopSineWave();

	return UcAdjSts;
}
#endif

unsigned char BiasOffsetAdj(unsigned char UcJikuSel, unsigned char UcMeasCnt)
{
	unsigned char UcHadjRst;

	unsigned long UlTgtVal[2][5] = {{0x3F800000, 0x3D200140, 0xBD200140,
					 0x3F547AE1, 0x3F451EB8}, /* ROUGH */
					{0x3F000000, 0x3D200140, 0xBD200140,
					 0x3F50A3D7, 0x3F48F5C3} }; /* FINE */

	if (UcMeasCnt > 1)
		UcMeasCnt = 1;

	UcJikuSel &= 0x01;

	/* Set Sine Wave */
	SetSineWave(UcJikuSel, __MEASURE_BIASOFFSET);

	/* Measure count */
	RegWriteA_LC898122AF(WC_MESLOOP1, 0x00); /* 0x0193 */
	RegWriteA_LC898122AF(WC_MESLOOP0,
			     0x04); /* 0x0192       4 Times Measure */
	RamWrite32A_LC898122AF(
		msmean, 0x3E000000); /* 0x10AE       1/CmMesLoop[15:0]/2 */
	RegWriteA_LC898122AF(WC_MESABS, 0x00); /* 0x0198       ABS */

	/* Set Adjustment Limits */
	RamWrite32A_LC898122AF(
		HOStp,
		UlTgtVal[UcMeasCnt][0]); /* 0x1070       Hall Offset Stp */
	RamWrite32A_LC898122AF(
		HOMax, UlTgtVal[UcMeasCnt]
			       [1]); /* 0x1072       Hall Offset max limit */
	RamWrite32A_LC898122AF(
		HOMin, UlTgtVal[UcMeasCnt]
			       [2]); /* 0x1071       Hall Offset min limit */
	RamWrite32A_LC898122AF(
		HBStp, UlTgtVal[UcMeasCnt][0]); /* 0x1080       Hall Bias Stp */
	RamWrite32A_LC898122AF(
		HBMax,
		UlTgtVal[UcMeasCnt][3]); /* 0x1082       Hall Bias max limit */
	RamWrite32A_LC898122AF(
		HBMin,
		UlTgtVal[UcMeasCnt][4]); /* 0x1081       Hall Bias min limit */

	RegWriteA_LC898122AF(WC_AMJLOOP1,
			     0x00); /* 0x01A3       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJLOOP0,
			     0x40);		/* 0x01A2       Time-Out time */
	RegWriteA_LC898122AF(WC_AMJIDL1, 0x00); /* 0x01A5       wait */
	RegWriteA_LC898122AF(WC_AMJIDL0, 0x00); /* 0x01A4       wait */

	/* Set Measure Filter */
	SetMeasFil(HALL_ADJ);

	/* Clear Measure Filters */
	ClrMeasFil();

	/* Start Sine Wave */
	StartSineWave();

	/* Check Busy Flag */
	BsyWit(
		WC_AMJMODE,
		(0x0C |
		 (UcJikuSel
		  << 4))); /* 0x01A0       Hall bais/offset ppt adjustment */

	RegReadA_LC898122AF(RC_AMJERROR, &UcAdjBsy); /* 0x01AD */

	if (UcAdjBsy) {
		if (UcJikuSel == X_DIR)
			UcHadjRst = EXE_HXADJ;
		else
			UcHadjRst = EXE_HYADJ;
	} else {
		UcHadjRst = EXE_END;
	}

	/* Stop Sine Wave */
	StopSineWave();

	/* Set Servo Filter */

	/* Ram Access */
	RamAccFixMod(ON); /* Fix mode */

	if (UcJikuSel == X_DIR) {
		RamReadA_LC898122AF(
			MSPP1AV,
			&StAdjPar.StHalAdj
				 .UsHlxMxa); /* 0x1042 Max width value */
		RamReadA_LC898122AF(
			MSCT1AV,
			&StAdjPar.StHalAdj.UsHlxCna); /* 0x1052 offset value */
	} else {
		RamReadA_LC898122AF(
			MSPP1AV,
			&StAdjPar.StHalAdj
				 .UsHlyMxa); /* 0x1042 Max width value */
		RamReadA_LC898122AF(
			MSCT1AV,
			&StAdjPar.StHalAdj.UsHlyCna); /* 0x1052 offset value */
	}

	StAdjPar.StHalAdj.UsHlxCna =
		(unsigned short)((signed short)StAdjPar.StHalAdj.UsHlxCna << 1);
	StAdjPar.StHalAdj.UsHlyCna =
		(unsigned short)((signed short)StAdjPar.StHalAdj.UsHlyCna << 1);
	/* Ram Access */
	RamAccFixMod(OFF); /* Float mode */

	return UcHadjRst;
}

#endif

void SetPanTiltMode(unsigned char UcPnTmod)
{
	switch (UcPnTmod) {
	case OFF:
		RegWriteA_LC898122AF(WG_PANON, 0x00);
		break;
	case ON:
		RegWriteA_LC898122AF(WG_PANON, 0x01);
		break;
	}
}

#ifdef GAIN_CONT

#endif


#ifdef NEUTRAL_CENTER

#endif /* NEUTRAL_CENTER */

void SetGcf(unsigned char UcSetNum)
{

	/* Zoom Step */
	if (UcSetNum > (COEFTBL - 1))
		UcSetNum = (COEFTBL - 1); /* è„å¿ÇCOEFTBL-1Ç…ê›íËÇ∑ÇÈ */

	UlH1Coefval = ClDiCof[UcSetNum];

	/* Zoom Value Setting */
	RamWrite32A_LC898122AF(gxh1c, UlH1Coefval); /* 0x1012 */
	RamWrite32A_LC898122AF(gyh1c, UlH1Coefval); /* 0x1112 */

#ifdef H1COEF_CHANGER
	SetH1cMod(UcSetNum); /* Re-setting */
#endif
}

#ifdef H1COEF_CHANGER

void SetH1cMod(unsigned char UcSetNum)
{

	switch (UcSetNum) {
	case (ACTMODE):		  /* initial */
		IniPtMovMod(OFF); /* Pan/Tilt setting (Still) */

		/* enable setting */
		/* Zoom Step */
		UlH1Coefval = ClDiCof[0];

		UcH1LvlMod = 0;

		/* Limit value Value Setting */
		RamWrite32A_LC898122AF(gxlmt6L, MINLMT); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gxlmt6H, MAXLMT); /* 0x102E H-Limit */

		RamWrite32A_LC898122AF(gylmt6L, MINLMT); /* 0x112D L-Limit */
		RamWrite32A_LC898122AF(gylmt6H, MAXLMT); /* 0x112E H-Limit */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF); /* 0x10AA Change coefficient gain */

		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF); /* 0x11AA Change coefficient gain */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;

	case (S2MODE): /* cancel lvl change mode */
		RegWriteA_LC898122AF(
			WG_HCHR, 0x10); /* 0x011B       GmHChrOn[1]=0 Sw OFF */
		break;

	case (MOVMODE):		 /* Movie mode */
		IniPtMovMod(ON); /* Pan/Tilt setting (Movie) */

		RamWrite32A_LC898122AF(gxlmt6L,
				       MINLMT_MOV); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gylmt6L,
				       MINLMT_MOV); /* 0x112D L-Limit */

		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF_MOV); /* 0x10AA Change coefficient gain */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF_MOV); /* 0x11AA Change coefficient gain */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;

	default:
		IniPtMovMod(OFF); /* Pan/Tilt setting (Still) */

		UcH1LvlMod = UcSetNum;

		RamWrite32A_LC898122AF(gxlmt6L, MINLMT); /* 0x102D L-Limit */
		RamWrite32A_LC898122AF(gylmt6L, MINLMT); /* 0x112D L-Limit */

		RamWrite32A_LC898122AF(
			gxmg, CHGCOEF); /* 0x10AA Change coefficient gain */
		RamWrite32A_LC898122AF(
			gymg, CHGCOEF); /* 0x11AA Change coefficient gain */

		RamWrite32A_LC898122AF(gxhc_tmp,
				       UlH1Coefval); /* 0x100E Base Coef */
		RamWrite32A_LC898122AF(gyhc_tmp,
				       UlH1Coefval); /* 0x110E Base Coef */

		RegWriteA_LC898122AF(
			WG_HCHR, 0x12); /* 0x011B       GmHChrOn[1]=1 Sw ON */
		break;
	}
}
#endif
