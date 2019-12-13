/*
 * rt5683.c  --  RT5683 ALSA SoC component driver
 *
 * Copyright 2018 Realtek Semiconductor Corp.
 * Author: Jack Yu <jack.yu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/acpi.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include "rt5683.h"
#define FixedType
/**
* Below gpio number is based on realtek's dev platform, and should be
* modified according to dev platform respectivelly or parsing from DTS.
*/
#define JACK_BTN_IRQ_GPIO 165

struct rt5683_priv {
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct snd_soc_jack *hs_jack;
	struct delayed_work hs_btn_detect_work;
	int sysclk;
	int sysclk_src;
	int lrck;
	int bclk;
	int master;
	int control;
	int pll_src;
	int pll_in;
	int pll_out;
	int g_PlabackHPStatus;
	int jack_type;
	int jd_status;
};

static const struct reg_default rt5683_reg[] = {
	{ 0x0000, 0x00 },
	{ 0x0001, 0x88 },
	{ 0x0002, 0x00 },
	{ 0x0003, 0x22 },
	{ 0x0004, 0x80 },
	{ 0x0005, 0x00 },
	{ 0x0006, 0x00 },
	{ 0x000b, 0x00 },
	{ 0x000c, 0x00 },
	{ 0x000d, 0x00 },
	{ 0x000e, 0x08 },
	{ 0x000f, 0x08 },
	{ 0x0010, 0x1f },
	{ 0x0011, 0x00 },
	{ 0x0017, 0x10 },
	{ 0x0019, 0x00 },
	{ 0x001a, 0xff },
	{ 0x001b, 0x00 },
	{ 0x001c, 0x55 },
	{ 0x001d, 0x88 },
	{ 0x001e, 0xaf },
	{ 0x001f, 0xaf },
	{ 0x0020, 0xc0 },
	{ 0x0021, 0x00 },
	{ 0x0022, 0xcc },
	{ 0x0023, 0x00 },
	{ 0x0024, 0x00 },
	{ 0x0025, 0x00 },
	{ 0x0026, 0x00 },
	{ 0x0027, 0x00 },
	{ 0x0028, 0x00 },
	{ 0x0029, 0x88 },
	{ 0x002a, 0x00 },
	{ 0x002b, 0xaf },
	{ 0x002d, 0xa0 },
	{ 0x0030, 0x00 },
	{ 0x0031, 0x00 },
	{ 0x0032, 0x00 },
	{ 0x0033, 0x00 },
	{ 0x0034, 0x00 },
	{ 0x0039, 0x00 },
	{ 0x003a, 0x00 },
	{ 0x003b, 0x00 },
	{ 0x003c, 0x00 },
	{ 0x003d, 0x00 },
	{ 0x003e, 0x00 },
	{ 0x003f, 0x00 },
	{ 0x0040, 0x7f },
	{ 0x0041, 0x00 },
	{ 0x0042, 0x00 },
	{ 0x0043, 0x00 },
	{ 0x0044, 0x7f },
	{ 0x0045, 0x00 },
	{ 0x0046, 0x00 },
	{ 0x0047, 0x00 },
	{ 0x0048, 0x7f },
	{ 0x0049, 0x0c },
	{ 0x004a, 0x0c },
	{ 0x0060, 0x02 },
	{ 0x0061, 0x00 },
	{ 0x0062, 0x00 },
	{ 0x0063, 0x02 },
	{ 0x0064, 0xff },
	{ 0x0065, 0x00 },
	{ 0x0066, 0x61 },
	{ 0x0067, 0x05 },
	{ 0x0068, 0x00 },
	{ 0x0069, 0x00 },
	{ 0x006b, 0x00 },
	{ 0x008e, 0xc2 },
	{ 0x008f, 0x04 },
	{ 0x0090, 0x0c },
	{ 0x0091, 0x26 },
	{ 0x0092, 0x30 },
	{ 0x0093, 0x73 },
	{ 0x0094, 0x10 },
	{ 0x0095, 0x04 },
	{ 0x0096, 0x00 },
	{ 0x0097, 0x00 },
	{ 0x0098, 0x00 },
	{ 0x0099, 0x00 },
	{ 0x00b0, 0x00 },
	{ 0x00b1, 0x00 },
	{ 0x00b2, 0x00 },
	{ 0x00b3, 0x00 },
	{ 0x00b4, 0x00 },
	{ 0x00b5, 0x00 },
	{ 0x00b6, 0x00 },
	{ 0x00b7, 0x00 },
	{ 0x00b8, 0x00 },
	{ 0x00b9, 0x00 },
	{ 0x00ba, 0x00 },
	{ 0x00bb, 0x02 },
	{ 0x00bd, 0x00 },
	{ 0x00be, 0x00 },
	{ 0x00bf, 0x00 },
	{ 0x00c0, 0x00 },
	{ 0x00d0, 0x00 },
	{ 0x00d1, 0x22 },
	{ 0x00d2, 0x04 },
	{ 0x00d3, 0x00 },
	{ 0x00d4, 0x00 },
	{ 0x00d5, 0x33 },
	{ 0x00d6, 0x00 },
	{ 0x00d7, 0x22 },
	{ 0x00d8, 0x00 },
	{ 0x00d9, 0x09 },
	{ 0x00da, 0x00 },
	{ 0x00e0, 0x00 },
	{ 0x00f0, 0x00 },
	{ 0x00f1, 0x00 },
	{ 0x00f2, 0x00 },
	{ 0x00f3, 0x00 },
	{ 0x00f6, 0x00 },
	{ 0x00f7, 0x00 },
	{ 0x00f8, 0x00 },
	{ 0x00f9, 0x00 },
	{ 0x00fa, 0x00 },
	{ 0x00fb, 0x10 },
	{ 0x00fc, 0xec },
	{ 0x00fd, 0x01 },
	{ 0x00fe, 0x65 },
	{ 0x00ff, 0x40 },
	{ 0x0109, 0x34 },
	{ 0x013A, 0x20 },
	{ 0x013B, 0x22 },
	{ 0x0194, 0x00 },
	{ 0x01DB, 0x04 },
	{ 0x01DC, 0x04 },
	{ 0x0208, 0x00 },
	{ 0x0210, 0x00 },
	{ 0x0211, 0x00 },
	{ 0x0213, 0x00 },
	{ 0x0214, 0x00 },
	{ 0x0703, 0x00 },
	{ 0x070c, 0x00 },
	{ 0x070d, 0x00 },
	{ 0x070e, 0x40 },
	{ 0x071a, 0xaf },
	{ 0x071b, 0xaf },
	{ 0x0e03, 0x2f },
	{ 0x0e04, 0x2f },
	{ 0x1b05, 0x00 },
	{ 0x2B00, 0x42 },
	{ 0x2B01, 0x40 },
	{ 0x2B02, 0x00 },
	{ 0x2B03, 0x00 },
	{ 0x2B05, 0x04 },
	{ 0x3300, 0x40 },
	{ 0x3303, 0xa0 },
	{ 0x3312, 0x00 },
	{ 0x3316, 0x00 },
	{ 0x3317, 0x00 },
	{ 0x3a00, 0x01 },
};

static bool rt5683_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0000:
	case 0x0029:
	case 0x00b6:
	case 0x00bd:
	case 0x00be:
	case 0x00f0:
	case 0x00f1:
	case 0x00f2:
	case 0x00f3:
	case 0x00f9:
	case 0x00fa:
	case 0x00fb:
	case 0x00fc:
	case 0x00fd:
	case 0x00fe:
	case 0x00ff:
	case 0x070c:
	case 0x070d:
	case 0x2b01:
	case 0x2b02:
	case 0x2b03:
	case 0x3303:
	case 0x3312:
	case 0x3316:
	case 0x3317:
		return true;

	default:
		return false;
	}
}

static bool rt5683_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x0000 ... 0x0006:
	case 0x000b ... 0x0011:
	case 0x0017:
	case 0x0019 ... 0x002b:
	case 0x002d:
	case 0x0030 ... 0x0034:
	case 0x0039 ... 0x004a:
	case 0x0060 ... 0x0069:
	case 0x006b:
	case 0x008e:
	case 0x008f:
	case 0x0090 ... 0x0099:
	case 0x00b0 ... 0x00bb:
	case 0x00bd ... 0x00c0:
	case 0x00d0 ... 0x00da:
	case 0x00e0:
	case 0x00f0 ... 0x00f3:
	case 0x00f6 ... 0x00ff:
	case 0x0109:
	case 0x013a:
	case 0x013b:
	case 0x0194:
	case 0x01db:
	case 0x01dc:
	case 0x0208:
	case 0x0210:
	case 0x0211:
	case 0x0213:
	case 0x0214:
	case 0x0703:
	case 0x070c:
	case 0x070d:
	case 0x070e:
	case 0x071a:
	case 0x071b:
	case 0x0e03:
	case 0x0e04:
	case 0x1b05:
	case 0x2b00:
	case 0x2b01:
	case 0x2b02:
	case 0x2b03:
	case 0x2b05:
	case 0x3300:
	case 0x3303:
	case 0x3312:
	case 0x3316:
	case 0x3317:
	case 0x3a00:
		return true;
	default:
		return false;
	}
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -65625, 375, 0);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -17625, 375, 0);

static void rt5683_CodecPowerBack(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	
	regmap_update_bits(rt5683->regmap,0x0208,0x01,0x01);//PowerON   - sysclk

	regmap_update_bits(rt5683->regmap,0x0063,0xAE,0xAE); //PowerOn   - Fast VREF for performance + Enable MBIAS/Bandgap
	msleep(3);
	regmap_update_bits(rt5683->regmap,0x0063,0xFE,0xFE); //PowerOn   - Slow VREF for performance + Enable MBIAS/Bandgap
	regmap_update_bits(rt5683->regmap,0x0061,0x63,0x63); //PowerOn   - LDO_DACREF/DACL1/DACR1/ADCL1
	#ifdef FixedType
	regmap_update_bits(rt5683->regmap,0x0062,0xCC,0xC0); //PowerOn - BST1 Power & MICBIAS1/MICBIAS2 for CBJ          
	regmap_update_bits(rt5683->regmap,0x0065,0x61,0x61); //Keep    - LDO2/LDO_I2S 
	regmap_update_bits(rt5683->regmap,0x0214,0xFA,0xFA); //PowerOn - HPSequence/SAR_ADC/ (Here is for depop)
	#else
	regmap_update_bits(rt5683->regmap,0x0062,0x0C,0x0C); //PowerOn - MICBIAS1/MICBIAS2 for CBJ
	regmap_update_bits(rt5683->regmap,0x0065,0xE1,0xE1); //Keep    - BJ/LDO2/LDO_I2S    
	regmap_update_bits(rt5683->regmap,0x0214,0xFA,0xFA); //PowerOn - HPSequence/SAR_ADC/ComboJD (Here is for depop)
	#endif            
	regmap_update_bits(rt5683->regmap,0x0068,0x03,0x03); //PowerOn   - 1M/25M OSC 
	regmap_update_bits(rt5683->regmap,0x0069,0x80,0x80); //PowerOn   - RECMIX1L
	regmap_update_bits(rt5683->regmap,0x0210,0xA3,0xA3); //PowerOn   - ADC Filter/DAC Filter/DAC Mixer
	regmap_update_bits(rt5683->regmap,0x0211,0x01,0x01); //PowerOn   - DSP post VOL
	regmap_update_bits(rt5683->regmap,0x0213,0xC0,0xC0); //PowerOn   - Silence Detect on DA Stereo
	regmap_update_bits(rt5683->regmap,0x013A,0x10,0x10); //PowerOn   - Enable DAC Clock
	regmap_update_bits(rt5683->regmap,0x013B,0x11,0x11); //PowerOn   - Enable ADC1/ADC2 Clock       
	msleep(5);
}

static void rt5683_CodecPowerSaving(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	
	regmap_update_bits(rt5683->regmap,RT5683_HP_SIG_SRC_CTRL,Sel_hp_sig_sour1,ByRegister); //Depop
	regmap_update_bits(rt5683->regmap,0x0063,0xFE,0x00); //PowerOFF   - Slow VREF for performance + Enable MBIAS/Bandgap
	regmap_update_bits(rt5683->regmap,0x0061,0x63,0x00); //PowerOFF   - LDO_DACREF/DACL1/DACR1/ADCL1/ADCR1
	#ifdef FixedType
	regmap_update_bits(rt5683->regmap,0x0062,0xCC,0x00); //PowerOFF - BST1 Power & MICBIAS1/MICBIAS2 for CBJ
	regmap_update_bits(rt5683->regmap,0x0065,0x61,0x61); //Keep     - LDO2/LDO_I2S
	regmap_update_bits(rt5683->regmap,0x0214,0xFA,0x9A); //Keep     - InLine Detect Power               
	#else
	regmap_update_bits(rt5683->regmap,0x0062,0x0C,0x00); //PowerOFF - MICBIAS1/MICBIAS2 for CBJ
	regmap_update_bits(rt5683->regmap,0x0065,0xE1,0xE1); //Keep     - BJ/LDO2/LDO_I2S
	regmap_update_bits(rt5683->regmap,0x0214,0xFA,0x9A); //Keep     - InLine Detect Power               
	#endif                    
	regmap_update_bits(rt5683->regmap,0x0068,0x03,0x03); //Keep       - 1M/25M OSC    
	regmap_update_bits(rt5683->regmap,0x0069,0x80,0x00); //PowerOFF   - RECMIX1L
	regmap_update_bits(rt5683->regmap,0x013A,0x10,0x00); //PowerOFF   - Enable DAC Clock
	regmap_update_bits(rt5683->regmap,0x013B,0x11,0x00); //PowerOFF   - Enable ADC1/ADC2 Clock       
	regmap_update_bits(rt5683->regmap,0x0208,0x01,0x00); //PowerOFF   - sysclk
	regmap_update_bits(rt5683->regmap,0x0210,0xA3,0x00); //PowerOFF   - ADC Filter/DAC Filter/DAC Mixer
	regmap_update_bits(rt5683->regmap,0x0211,0x01,0x00); //PowerOFF   - DSP post VOL
	regmap_update_bits(rt5683->regmap,0x0213,0xC0,0x00); //PowerOFF   - Silence Detect on DA Stereo
}

static const char *rt5683_ctrl_mode[] = {
	"None", "No Playback-Record","Playback+Record", "Only Playback", "Only Record",
};

static const SOC_ENUM_SINGLE_DECL(rt5683_dsp_mod_enum, 0, 0,
	rt5683_ctrl_mode);

static int rt5683_control_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	unsigned int silence_det;

	rt5683->control = ucontrol->value.integer.value[0];
	regmap_read(rt5683->regmap, 0x1B05, &silence_det);

	/**
	* "RT5683 Control" description
	* 0: None
	* 1: No Playback +No Recording
	* 2: Playback +Recording
	* 3: Only Playback
	* 4: Only Recording
	*/
	if (rt5683->control == 0) {
		pr_info("RT5683 Control None\n");
	} else if (rt5683->control == 1) {
		if (silence_det == 0x55){
			regmap_update_bits(rt5683->regmap,0x008E,0xFF,0x00);
			rt5683->g_PlabackHPStatus=0;
		} else {
			regmap_update_bits(rt5683->regmap,0x01DB,Sel_hp_sig_sour1,ByRegister);
			regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
			regmap_update_bits(rt5683->regmap,0x008E,0xE0,0x00); //Disable EN_OUT_HP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x0061,0x03,0x00); //Disable POW_DAC
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x08,0x00); //Disable POW_CAPLESS
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x10,0x00); //Disable POW_PUMP
			msleep(5); 
			rt5683->g_PlabackHPStatus=0;    
		}
		rt5683_CodecPowerSaving(component);
		regmap_update_bits(rt5683->regmap,0xFA34,0x01,0x01);  //Enable reg_en_ep_clkgat for power saving
		regmap_update_bits(rt5683->regmap,0x0109,0x70,0x40);  //BUCK=1.95V
		regmap_update_bits(rt5683->regmap,0x2B05,0x80,0x00);  //Disable [EN_IBUF_CBJ_BST1]  for Power Saving
		regmap_update_bits(rt5683->regmap,0x0194,0x85,0x05);   //Disable - HP Auto Mute/UnMute - On/Off by Silence Detect
		pr_info("No Playback +No Recording\n");
	} else if (rt5683->control == 2) {
		if(rt5683->g_PlabackHPStatus == 0)
		{
			if(silence_det == 0x55)
			{
				regmap_update_bits(rt5683->regmap,0x008E,0xFF,0x00);
				rt5683->g_PlabackHPStatus=0;   
			}
			else
			{
				regmap_update_bits(rt5683->regmap,0x01DB,Sel_hp_sig_sour1,ByRegister);
				regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
				regmap_update_bits(rt5683->regmap,0x008E,0xE0,0x00); //Disable EN_OUT_HP
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x0061,0x03,0x00); //Disable POW_DAC
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x008E,0x08,0x00); //Disable POW_CAPLESS
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x008E,0x10,0x00); //Disable POW_PUMP
				msleep(5); 
				rt5683->g_PlabackHPStatus=0;
			} 
		}
			rt5683_CodecPowerBack(component);
			regmap_update_bits(rt5683->regmap,0x0061,0x20,0x20);    //only need ADC1L
			regmap_update_bits(rt5683->regmap,0x0210,0x80,0x80);
			regmap_update_bits(rt5683->regmap,0x0069,0x80,0x80); 
			regmap_update_bits(rt5683->regmap,0x3A00,0x80,0x80);
			regmap_update_bits(rt5683->regmap,0x00F9 ,0xFF,0x84); //Toggle Clear SPKVDD Auto Recovery Error Flag during Power Saving
			msleep(1); 
			regmap_update_bits(rt5683->regmap,0x00F9 ,0xFF,0x04);              
			regmap_update_bits(rt5683->regmap,0x0109 ,0x70,0x40);  //BUCK=1.95V
			regmap_update_bits(rt5683->regmap,0x2B05 ,0x80,0x80);  //Recovery [EN_IBUF_CBJ_BST1]  for Power Saving 
		if(rt5683->g_PlabackHPStatus == 0)
		{
			regmap_update_bits(rt5683->regmap,0x01DB,Sel_hp_sig_sour1,ByRegister);
			regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
			regmap_update_bits(rt5683->regmap,0x008E,0x10,0x10); //Enable POW_PUMP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x08,0x08); //Enable POW_CAPLESS
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x0061,0x03,0x03); //Enable POW_DAC
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x20,0x20); //Enable EN_OUT_HP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0xE0,0xE0); //Enable EN_OUT_HP
			msleep(5); 
			regmap_update_bits(rt5683->regmap,0x01DB,Sel_hp_sig_sour1,SilenceDetect);
			rt5683->g_PlabackHPStatus=1;
		}
		regmap_update_bits(rt5683->regmap,0x0194,0x85,0x85);     //Enable - HP Auto Mute/UnMute - On/Off by Silence Detect
		pr_info("Playback +Recording\n");
	} else if (rt5683->control == 3) {
		if(rt5683->g_PlabackHPStatus == 0)
		{
			if(silence_det == 0x55)
			{
				regmap_update_bits(rt5683->regmap,0x008E,0xFF,0x00);
				rt5683->g_PlabackHPStatus=0;
			}
			else
			{
				regmap_update_bits(rt5683->regmap,0x01DB,Sel_hp_sig_sour1,ByRegister);
				regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
				regmap_update_bits(rt5683->regmap,0x008E,0xE0,0x00); //Disable EN_OUT_HP
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x0061,0x03,0x00); //Disable POW_DAC
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x008E,0x08,0x00); //Disable POW_CAPLESS
				msleep(5);
				regmap_update_bits(rt5683->regmap,0x008E,0x10,0x00); //Disable POW_PUMP
				msleep(5); 
				rt5683->g_PlabackHPStatus=0;
			}
		}
			rt5683_CodecPowerBack(component);
			regmap_update_bits(rt5683->regmap,0xFA34,0x01,0x00); //Disable reg_en_ep_clkgat to avoid no sound issue           
			regmap_update_bits(rt5683->regmap,0x0061,0x20,0x00);   //Power Down ADC1L
			regmap_update_bits(rt5683->regmap,0x0069,0x80,0x00);   //Power Down RECMIX1_L
			
			regmap_update_bits(rt5683->regmap,0x00F9 ,0xFF,0x84);  //Toggle Clear SPKVDD Auto Recovery Error Flag during Power Saving 
			msleep(1); 
			regmap_update_bits(rt5683->regmap,0x00F9 ,0xFF,0x04);
			regmap_update_bits(rt5683->regmap,0x0109 ,0x70,0x40);  //BUCK=1.95V
			regmap_update_bits(rt5683->regmap,0x2B05 ,0x80,0x00);  //Disable [EN_IBUF_CBJ_BST1]  for Power Saving 
		if(rt5683->g_PlabackHPStatus == 0)
		{
			regmap_update_bits(rt5683->regmap,0x01BD,Sel_hp_sig_sour1,ByRegister);
			regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
			regmap_update_bits(rt5683->regmap,0x008E,0x10,0x10); //Enable POW_PUMP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x08,0x08); //Enable POW_CAPLESS
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x0061,0x03,0x03); //Enable POW_DAC
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x20,0x20); //Enable EN_OUT_HP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0xE0,0xE0); //Enable EN_OUT_HP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x01BD,Sel_hp_sig_sour1,SilenceDetect);
			rt5683->g_PlabackHPStatus=1;
		}
			regmap_update_bits(rt5683->regmap,0x0194,0x85,0x05);   //Disable - HP Auto Mute/UnMute - On/Off by Silence Detect
			pr_info("Only Playback\n");
	} else if (rt5683->control == 4) {
		if(silence_det == 0x55)
		{
			regmap_update_bits(rt5683->regmap,0x008E,0xFF,0x00);
			rt5683->g_PlabackHPStatus=0;
		}
		else
		{
			regmap_update_bits(rt5683->regmap,0x01BD,Sel_hp_sig_sour1,ByRegister);
			regmap_update_bits(rt5683->regmap,0x01DC,0x04,0x04);
			regmap_update_bits(rt5683->regmap,0x008E,0xE0,0x00); //Disable EN_OUT_HP
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x0061,0x03,0x00); //Disable POW_DAC
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x08,0x00); //Disable POW_CAPLESS
			msleep(5);
			regmap_update_bits(rt5683->regmap,0x008E,0x10,0x00); //Disable POW_PUMP
			msleep(5); 
			rt5683->g_PlabackHPStatus=0;
		}
		rt5683_CodecPowerBack(component);
		regmap_update_bits(rt5683->regmap,0x0061,0x20,0x20);//only need ADC1L
		regmap_update_bits(rt5683->regmap,0x0210,0x80,0x80);
		regmap_update_bits(rt5683->regmap,0x0069,0x80,0x80);
		regmap_update_bits(rt5683->regmap,0x3A00,0x80,0x80);
		regmap_update_bits(rt5683->regmap,0x0109, 0x70,0x40);//BUCK=1.95V
		regmap_update_bits(rt5683->regmap,0x2B05 ,0x80,0x80);//Recovery [EN_IBUF_CBJ_BST1]  for Power Saving 
		regmap_update_bits(rt5683->regmap,0x0194,0x85,0x05);//Disable - HP Auto Mute/UnMute - On/Off by Silence Detect
		pr_info("Only Record\n");
	}

	return 0;
}

static int rt5683_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = rt5683->control;

	return 0;
}

static const struct snd_kcontrol_new rt5683_snd_controls[] = {
	SOC_SINGLE_TLV("DACL Playback Volume", RT5683_L_CH_VOL_DAC,
		0, 175, 0, dac_vol_tlv),
	SOC_SINGLE_TLV("DACR Playback Volume", RT5683_R_CH_VOL_DAC,
		0, 175, 0, dac_vol_tlv),
	SOC_SINGLE_TLV("ADCL Playback Volume", RT5683_L_CH_VOL_ADC,
		0, 127, 0, adc_vol_tlv),
	SOC_SINGLE_TLV("ADCR Playback Volume", RT5683_R_CH_VOL_ADC,
		0, 127, 0, adc_vol_tlv),
	SOC_ENUM_EXT("RT5683 Control", rt5683_dsp_mod_enum, rt5683_control_get,
		rt5683_control_put),
};

static const struct snd_soc_dapm_widget rt5683_dapm_widgets[] = {
	
	SND_SOC_DAPM_AIF_IN("AIF1RX", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF1TX", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIF2RX", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF2TX", "AIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_INPUT("IN1P"),
	SND_SOC_DAPM_INPUT("IN1N"),
	SND_SOC_DAPM_INPUT("IN2P"),
	SND_SOC_DAPM_INPUT("IN2N"),
	SND_SOC_DAPM_OUTPUT("HPOL"),
	SND_SOC_DAPM_OUTPUT("HPOR"),
};

static const struct snd_soc_dapm_route rt5683_dapm_routes[] = {
	{ "AIF1TX", NULL, "IN1P" },
	{ "AIF1TX", NULL, "IN1N" },
	{ "AIF1TX", NULL, "IN2P" },
	{ "AIF1TX", NULL, "IN2N" },
	{ "AIF2TX", NULL, "IN1P" },
	{ "AIF2TX", NULL, "IN1N" },
	{ "AIF2TX", NULL, "IN2P" },
	{ "AIF2TX", NULL, "IN2N" },
	{ "HPOL", NULL, "AIF1RX" },
	{ "HPOR", NULL, "AIF1RX" },
	{ "HPOL", NULL, "AIF2RX" },
	{ "HPOR", NULL, "AIF2RX" },
};

static int rt5683_probe(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);

	rt5683->component = component;
	rt5683->jack_type = 0;
	rt5683->jd_status = 0x30;

	return 0;
}

#ifdef CONFIG_PM
static int rt5683_suspend(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);

	regcache_cache_only(rt5683->regmap, true);
	regcache_mark_dirty(rt5683->regmap);

	return 0;
}

static int rt5683_resume(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);

	regcache_cache_only(rt5683->regmap, false);
	regcache_sync(rt5683->regmap);

	return 0;
}
#else
#define rt5683_suspend NULL
#define rt5683_resume NULL
#endif

#define RT5683_STEREO_RATES SNDRV_PCM_RATE_8000_192000
#define RT5683_FORMATS (SNDRV_PCM_FMTBIT_S8 | \
			SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver rt5683_dai[] = {
	{
		.name = "rt5683-aif1",
		.id = RT5683_AIF1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5683_STEREO_RATES,
			.formats = RT5683_FORMATS,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5683_STEREO_RATES,
			.formats = RT5683_FORMATS,
		},
	},
	{
		.name = "rt5683-aif2",
		.id = RT5683_AIF2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5683_STEREO_RATES,
			.formats = RT5683_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = RT5683_STEREO_RATES,
			.formats = RT5683_FORMATS,
		},
	},
};

static const struct snd_soc_component_driver soc_component_dev_rt5683 = {
	.probe = rt5683_probe,
	.suspend = rt5683_suspend,
	.resume = rt5683_resume,
	.controls = rt5683_snd_controls,
	.num_controls = ARRAY_SIZE(rt5683_snd_controls),
	.dapm_widgets = rt5683_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(rt5683_dapm_widgets),
	.dapm_routes = rt5683_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(rt5683_dapm_routes),
	.use_pmdown_time	= 1,
	.endianness		= 1,
	.non_legacy_dai_naming	= 1,
};

static const struct regmap_config rt5683_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register = RT5683_PHY_CTRL_27,
	.volatile_reg = rt5683_volatile_register,
	.readable_reg = rt5683_readable_register,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rt5683_reg,
	.num_reg_defaults = ARRAY_SIZE(rt5683_reg),
	.use_single_rw = true,
};

#if defined(CONFIG_OF)
static const struct of_device_id rt5683_of_match[] = {
	{ .compatible = "realtek,rt5683", },
	{},
};
MODULE_DEVICE_TABLE(of, rt5683_of_match);
#endif

#ifdef CONFIG_ACPI
static struct acpi_device_id rt5683_acpi_match[] = {
	{"10EC5683", 0,},
	{},
};
MODULE_DEVICE_TABLE(acpi, rt5683_acpi_match);
#endif

static const struct i2c_device_id rt5683_i2c_id[] = {
	{ "rt5683", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rt5683_i2c_id);

static irqreturn_t rt5683_hs_btn_irq_handler(int irq, void *dev_id)
{
	struct rt5683_priv *rt5683  = dev_id;

	pr_info("%s:HS BTN IRQ detected!",__func__);
	mod_delayed_work(system_power_efficient_wq,
			&rt5683->hs_btn_detect_work, msecs_to_jiffies(30));

	return IRQ_HANDLED;
}

int rt5683_set_jack_detect(struct snd_soc_component *component,
	struct snd_soc_jack *hs_jack)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);

	rt5683->hs_jack = hs_jack;
	rt5683_hs_btn_irq_handler(0, rt5683);

	return 0;
}
EXPORT_SYMBOL_GPL(rt5683_set_jack_detect);

int rt5683_button_detect(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	int btn_type = 0, val_00B6, val_070C, val3_070D;

	regmap_read(rt5683->regmap, 0x00B6, &val_00B6);
	regmap_read(rt5683->regmap, 0x070C, &val_070C);
	regmap_read(rt5683->regmap, 0x070D, &val3_070D);

	if ((val_00B6 & 0x10) == 0x00){
		if ((val_070C == 0x10) && (val3_070D == 0x00)  )       //4 Buttoms-1 (A-double Click)                   
		{
			regmap_write(rt5683->regmap, 0x070C, 0x11);           
			regmap_write(rt5683->regmap, 0x070D, 0x11);
			btn_type |= SND_JACK_BTN_0;
			pr_info("[DBG] 1, 0x00b6:0x%x\n",val_00B6);
		}
		else if ((val_070C == 0x00) && (val3_070D == 0x10) )   //4 Buttoms-3 (B-one Click) 
		{
			regmap_write(rt5683->regmap, 0x070C, 0x11);           
			regmap_write(rt5683->regmap, 0x070D, 0x11);
			btn_type |= SND_JACK_BTN_2;
			pr_info("[DBG] 2, 0x00b6:0x%x\n",val_00B6);
		}              
		else if ((val_070C == 0x00) && (val3_070D == 0x01) )   //4 Buttoms-4 (C-one Click)
		{
			regmap_write(rt5683->regmap, 0x070C, 0x11);           
			regmap_write(rt5683->regmap, 0x070D, 0x11);
			btn_type |= SND_JACK_BTN_3;
			pr_info("[DBG] 3, 0x00b6:0x%x\n",val_00B6);
		}               
		else if ((val_070C == 0x01) && (val3_070D == 0x00) )   //4 Buttoms-2 (D-one Click)
		{
			regmap_write(rt5683->regmap, 0x070C, 0x11);
			regmap_write(rt5683->regmap, 0x070D, 0x11);
			btn_type |= SND_JACK_BTN_1;
			pr_info("[DBG] 4, 0x00b6:0x%x\n",val_00B6);
		}
		else //Abnormal Bottom push
		{
			val_00B6 |= 0x80;
			regmap_write(rt5683->regmap, 0x00B6, val_00B6);
			regmap_write(rt5683->regmap, 0x070C, 0xFF); //Clear Flag (!!!!!! Need to clear all flag)  , Clear will become 0'b when user release press behavior           
			regmap_write(rt5683->regmap, 0x070D, 0xFF); //Clear Flag (!!!!!! Need to clear all flag)  , Clear will become 0'b when user release press behavior                                                            
			pr_info("[DBG] Abnormal Button push, 0x00b6:0x%x\n",val_00B6);
		}
	} else {
		pr_debug("Unknown value,val_00B6=0x%x\n",val_00B6);
		return -EINVAL;
	}

	return btn_type;
}
EXPORT_SYMBOL(rt5683_button_detect);

void rt5683_sar_adc_button_det(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	
	regmap_write(rt5683->regmap, 0x0011, 0xa7);
	regmap_write(rt5683->regmap, 0x3300, 0x85);
	msleep(50);
	regmap_update_bits(rt5683->regmap, 0x3300, 0x80, 0x0);
	regmap_update_bits(rt5683->regmap, 0x3300, 0x80, 0x80);
	msleep(50);
}

int rt5683_headset_detect(struct snd_soc_component *component)
{
	struct rt5683_priv *rt5683 = snd_soc_component_get_drvdata(component);
	int jack_type, val_2b03, sleep_loop=4;
	int i = 0, sleep_time[4] = {150, 100, 50, 25};

	regmap_update_bits(rt5683->regmap, 0x0068, 0x3, 0x03);
	regmap_write(rt5683->regmap, 0x0063, 0xFE);
	regmap_update_bits(rt5683->regmap, 0x0065, 0xE0, 0xE0);
	regmap_update_bits(rt5683->regmap, 0x0090, 0xC, 0x0);
	regmap_update_bits(rt5683->regmap, 0x0214, 0x18, 0x18);
	regmap_write(rt5683->regmap, 0x2B05, 0x80);
	regmap_write(rt5683->regmap, 0x2B02, 0x0C);
	regmap_write(rt5683->regmap, 0x2B03, 0x44);
	regmap_write(rt5683->regmap, 0x2B01, 0x00);
	regmap_write(rt5683->regmap, 0x0062, 0x0C);
	regmap_write(rt5683->regmap, 0x0063, 0xAE);
	regmap_write(rt5683->regmap, 0x2B00, 0xD0);
	regmap_write(rt5683->regmap, 0x2B03, 0x44);
	regmap_write(rt5683->regmap, 0x0011, 0x80);
	regmap_write(rt5683->regmap, 0x2b01, 0x0);
	msleep(10);
	regmap_write(rt5683->regmap, 0x2b01, 0x8);

	while(i < sleep_loop){
		msleep(sleep_time[i]);
		regmap_read(rt5683->regmap, 0x2B03, &val_2b03);
		val_2b03 &= 0x3;
		pr_info("%s:val_2b03=%d sleep %d\n",
			__func__, val_2b03, sleep_time[i]);
		i++;
		if (val_2b03 == 0x1 || val_2b03 == 0x2 || val_2b03 == 0x3)
			break;
		if (val_2b03 == 0x0){
			regmap_write(rt5683->regmap, 0x2b01, 0x0);
			msleep(10);
			regmap_write(rt5683->regmap, 0x2b01, 0x8);
		}
	}

	if (val_2b03 == 0x1 || val_2b03 == 0x2)
		jack_type = SND_JACK_HEADSET;
	else
		jack_type = SND_JACK_HEADPHONE;

	rt5683_sar_adc_button_det(rt5683->component);
	pr_info("val_2b03:0x%x, jack_type=%s\n",val_2b03,(jack_type == SND_JACK_HEADSET)?"HEADSET":"HEADPHONE");
	return jack_type;
}
EXPORT_SYMBOL_GPL(rt5683_headset_detect);

void rt5683_irq_interrupt_event(struct work_struct *work)
{
	struct rt5683_priv *rt5683 =
		container_of(work, struct rt5683_priv, hs_btn_detect_work.work);
	unsigned int val_00bd;
	unsigned int val_00be,val_070c,val_070d;
	int report=0, i, btn_type=0, jd_is_changed=0;
	
	for(i=0;i<3;i++){
		msleep(1);
		regmap_read(rt5683->regmap, 0x00BD, &val_00bd);
		dev_dbg(rt5683->component->dev, "val_00bd:0x%x\n",val_00bd);
	}
	if (rt5683->jd_status != val_00bd)
		jd_is_changed = 1;
	else
		jd_is_changed = 0;

	rt5683->jd_status = val_00bd;

	/* JD Status Confirm */
	if (((val_00bd & 0x30)==0x0)){
		
		if (jd_is_changed)
			rt5683->jack_type = rt5683_headset_detect(rt5683->component);
		
		report = rt5683->jack_type;
		regmap_update_bits(rt5683->regmap, 0x0214, 0x2, 0x2);
		regmap_read(rt5683->regmap, 0x00BE, &val_00be);
		regmap_read(rt5683->regmap, 0x070C, &val_070c);
		regmap_read(rt5683->regmap, 0x070D, &val_070d);
		val_070c &= 0x77;
		val_070d &= 0x77;

		/* Status of InLine Command Trigger */
		if (((val_00be & 0x80)==0x80) && !jd_is_changed){
			btn_type = rt5683_button_detect(rt5683->component);
			if (btn_type < 0)
				pr_err("Unexpected button code.\n");
			report |= btn_type;
		}
		if (btn_type == 0 || (val_070c == 0 && val_070d == 0)){
			pr_info("Button released.\n");
			regmap_write(rt5683->regmap, 0x070c, 0xff);
			regmap_write(rt5683->regmap, 0x070d, 0xff);
			report = rt5683->jack_type;
		}
	} else{
		pr_info("Unplug!\n");
		regmap_write(rt5683->regmap, 0x070c, 0xff);
		regmap_write(rt5683->regmap, 0x070d, 0xff);
		regmap_update_bits(rt5683->regmap, 0x3300, 0x80, 0x0);
		rt5683->jack_type = 0;
		report = 0;
	}

	snd_soc_jack_report(rt5683->hs_jack, report, SND_JACK_HEADSET |
		SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 |
		SND_JACK_BTN_3);
}

static int rt5683_i2c_probe(struct i2c_client *i2c,
		    const struct i2c_device_id *id)
{
	struct rt5683_priv *rt5683;
	unsigned int irq_num;
	int ret;

	rt5683 = devm_kzalloc(&i2c->dev, sizeof(struct rt5683_priv),
				GFP_KERNEL);
	if (rt5683 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, rt5683);

	rt5683->regmap = devm_regmap_init_i2c(i2c, &rt5683_regmap);
	if (IS_ERR(rt5683->regmap)) {
		ret = PTR_ERR(rt5683->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	INIT_DELAYED_WORK(&rt5683->hs_btn_detect_work, rt5683_irq_interrupt_event);

	irq_num = gpio_to_irq(JACK_BTN_IRQ_GPIO);
	ret = request_irq(irq_num, rt5683_hs_btn_irq_handler,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"5683_JDH", rt5683);
	if (ret < 0)
		dev_err(&i2c->dev, "JD IRQ request failed!\n");
	
	return devm_snd_soc_register_component(&i2c->dev,
			&soc_component_dev_rt5683,
			rt5683_dai, ARRAY_SIZE(rt5683_dai));
}

static struct i2c_driver rt5683_i2c_driver = {
	.driver = {
		.name = "rt5683",
#if defined(CONFIG_OF)
		.of_match_table = rt5683_of_match,
#endif
#if defined(CONFIG_ACPI)
		.acpi_match_table = ACPI_PTR(rt5683_acpi_match)
#endif
	},
	.probe = rt5683_i2c_probe,
	.id_table = rt5683_i2c_id,
};
module_i2c_driver(rt5683_i2c_driver);

MODULE_DESCRIPTION("ASoC RT5683 driver");
MODULE_AUTHOR("Jack Yu <jack.yu@realtek.com>");
MODULE_LICENSE("GPL v2");
