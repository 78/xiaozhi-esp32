#ifndef _NAU88C22_H_
#define _NAU88C22_H_

#define NAU88C22_I2C_ADDRESS 0x1a

enum e_gains
{
	gain_lauxin_to_lmainmix,
	gain_linmix_to_lmainmix,
	gain_lpga,
	gain_llin,
};

enum e_left_pga_src
{
	e_left_pga_mic,
	e_left_pga_lin
};
enum e_right_pga_src
{
	e_right_pga_mic,
	e_right_pga_lin
};
enum e_left_in_mix_srcs
{
	e_lim_Left_PGA,
	e_lim_LeftLine,
	e_lim_LeftAux
};
enum e_right_in_mix_srcs
{
	e_rim_Right_PGA,
	e_rim_RightLine,
	e_rim_RightAux
};
enum e_left_main_mix_srcs
{
	e_lmm_LeftAux,
	e_lmm_LeftInputMixer,
	e_lmm_LeftDAC,
	e_lmm_RightDAC
};
enum e_right_main_mix_srcs
{
	e_rmm_RightAux,
	e_rmm_RightInputMixer,
	e_rmm_RightDAC,
	e_rmm_LeftDAC
};
enum e_aux_1_mix_srcs
{
	e_a1m_LeftMainMixer,
	e_a1m_RightDAC,
	e_a1m_LeftDAC,
	e_a1m_RightInputMixer
};
// warning - aux2mixer is permanently connected to aux1mixer output
enum e_aux_2_mix_srcs
{
	e_a2m_LeftInputMixer,
	e_a2m_LeftDAC,
	e_a2m_LeftMainMixer
};
enum e_submix_srcs
{
	e_rs_RightMainMixer,
	e_rs_RightAux
};
enum e_mic_bias_levels
{
	e_mbl_off,
	e_mbl_85,
	e_mbl_70,
	e_mbl_60,
	e_mbl_50
}; // Vdda x _85 = 3.3V * 0.85 = 2.805V on mic bias pin
enum e_high_pass_filter
{
	e_hps_off,
	e_hps_113,
	e_hps_141,
	e_hpf_180,
	e_hpf_225,
	e_hpf_281,
	e_hpf_360,
	e_hpf_450,
	e_hpf_563
}; // cut-off freq for 44.1kHz operation
enum e_equ_band_1
{
	e_eb1_80,
	e_eb1_105,
	e_eb1_135,
	e_eb1_175
};
enum e_equ_band_2
{
	e_eb2_230,
	e_eb2_300,
	e_eb2_385,
	e_eb2_500
};
enum e_equ_band_3
{
	e_eb3_650,
	e_eb3_850,
	e_eb3_1100,
	e_eb3_1400
};
enum e_equ_band_4
{
	e_eb4_1800,
	e_eb4_2400,
	e_eb4_3200,
	e_eb4_4100
};
enum e_equ_band_5
{
	e_eb5_5300,
	e_eb5_6900,
	e_eb5_9000,
	e_eb5_11700
};
enum e_equ_src
{
	e_es_ADC,
	e_es_DAC
};
enum e_alc_channels
{
	e_alc_off,
	e_alc_left,
	e_alc_right,
	e_alc_booth
};

enum e_power_blocks
{
	e_pb_dcbufen,
	e_pb_aux1mxen,
	e_pb_aux2mxen,
	e_pb_pllen,
	e_pb_micbiasen,
	e_pb_abiasen,
	e_pb_iobufen,
	e_pb_refimp_off,
	e_pb_refimp_3k,
	e_pb_refimp_80k,
	e_pb_refimp_300k,
	e_pb_rhpen,
	e_pb_lhpen,
	e_pb_sleep,
	e_pb_rbesten,
	e_pb_lbesten,
	e_pb_rpgaen,
	e_pb_lpgaen,
	e_pb_radcen,
	e_pb_ladcen,
	e_pb_auxout1en,
	e_pb_auxout2en,
	e_pb_lspken,
	e_pb_rspken,
	e_pb_rmixen,
	e_pb_lmixen,
	e_pb_rdacen,
	e_pb_ldacen
};

typedef struct
{
	uint8_t dcbufen : 1;
	uint8_t aux1mxen : 1;
	uint8_t aux2mxen : 1;
	uint8_t pllen : 1;
	uint8_t micbiasen : 1;
	uint8_t abiasen : 1;
	uint8_t iobufen : 1;
	uint8_t refimp : 2;
} s_power_1;

typedef struct
{
	uint8_t rhpen : 1;
	uint8_t lhpen : 1;
	uint8_t sleep : 1;
	uint8_t rbsten : 1;
	uint8_t lbsten : 1;
	uint8_t rpgaen : 1;
	uint8_t lpgaen : 1;
	uint8_t radcen : 1;
	uint8_t ladcen : 1;
} s_power_2;

typedef struct
{
	uint8_t auxout1en : 1;
	uint8_t auxout2en : 1;
	uint8_t lspken : 1;
	uint8_t rspken : 1;
	uint8_t rmixen : 1;
	uint8_t lmixen : 1;
	uint8_t rdacen : 1;
	uint8_t ldacen : 1;
} s_power_3;

typedef struct
{
	uint8_t bclkp : 1;
	uint8_t lrp : 1;
	uint8_t wlen : 2;
	uint8_t aifmt : 2;
	uint8_t dacphs : 1;
	uint8_t adcphs : 1;
	uint8_t mono : 1;
} s_audio_interface;

typedef struct
{
	uint8_t cmb8 : 1;
	uint8_t daccm : 2;
	uint8_t adccm : 2;
	uint8_t addap : 1;
} s_companding;

typedef struct
{
	uint8_t clkm : 1;
	uint8_t mclksel : 3;
	uint8_t bclksel : 3;
	uint8_t clkioen : 1;
} s_clock_control_1;

typedef struct
{
	uint8_t wspien4 : 1;
	uint8_t smplr : 3;
	uint8_t sclken : 1;
} s_clock_control_2;

typedef struct
{
	uint8_t gpio1pll : 2;
	uint8_t gpio1pl : 1;
	uint8_t gpio1sel : 3;
} s_gpio;

typedef struct
{
	uint8_t jckmiden : 2;
	uint8_t jacden : 1;
	uint8_t jckdio : 2;
} s_jack_detect_1;

typedef struct
{
	uint8_t softmt : 1;
	uint8_t dacos : 1;
	uint8_t automt : 1;
	uint8_t rdacpl : 1;
	uint8_t ldacpl : 1;
} s_dac_control;

typedef struct
{
	uint8_t ldacvu : 1;
	uint8_t ldacgain : 8;
} s_left_dac_volume;

typedef struct
{
	uint8_t rdacvu : 1;
	uint8_t rdacgain : 8;
} s_right_dac_volume;

typedef struct
{
	uint8_t jckdoen1 : 4;
	uint8_t jckdoen0 : 4;
} s_jack_detect_2;

typedef struct
{
	uint8_t hpfen : 1;
	uint8_t hpfam : 1;
	uint8_t hpf : 3;
	uint8_t adcos : 1;
	uint8_t radcpl : 1;
	uint8_t ladcpl : 1;
} s_adc_control;

typedef struct
{
	uint8_t ladcvu : 1;
	uint8_t ladcgain : 8;
} s_left_adc_volume;

typedef struct
{
	uint8_t radcvu : 1;
	uint8_t radcgain : 8;
} s_right_adc_volume;

typedef struct
{
	uint8_t eqm : 1;
	uint8_t eq1cf : 2;
	uint8_t eq1gc : 5;
} s_eq1;

typedef struct
{
	uint8_t eq2bw : 1;
	uint8_t eq2cf : 2;
	uint8_t eq2gc : 5;
} s_eq2;

typedef struct
{
	uint8_t eq3bw : 1;
	uint8_t eq3cf : 2;
	uint8_t eq3gc : 5;
} s_eq3;

typedef struct
{
	uint8_t eq4bw : 1;
	uint8_t eq4cf : 2;
	uint8_t eq4gc : 5;
} s_eq4;

typedef struct
{
	uint8_t eq5cf : 2;
	uint8_t eq5gc : 5;
} s_eq5;

typedef struct
{
	uint8_t daclimen : 1;
	uint8_t daclimdcy : 4;
	uint8_t daclimatk : 4;
} s_dac_limiter_1;

typedef struct
{
	uint8_t daclimthl : 3;
	uint8_t daclimbst : 4;
} s_dac_limiter_2;

typedef struct
{
	uint8_t nfcu1 : 1;
	uint8_t nfcen : 1;
	uint8_t nfca0h : 7;
} s_notch_filter_1;

typedef struct
{
	uint8_t nfcu2 : 1;
	uint8_t nfca0l : 7;
} s_notch_filter_2;

typedef struct
{
	uint8_t nfcu3 : 1;
	uint8_t nfca1h : 7;
} s_notch_filter_3;

typedef struct
{
	uint8_t nfcu4 : 1;
	uint8_t nfca1l : 7;
} s_notch_filter_4;

typedef struct
{
	uint8_t alcen : 2;
	uint8_t alcmxgain : 3;
	uint8_t alcmngain : 3;
} s_alc_control_1;

typedef struct
{
	uint8_t alcht : 4;
	uint8_t alcsl : 4;
} s_alc_control_2;

typedef struct
{
	uint8_t alcm : 1;
	uint8_t alcdcy : 4;
	uint8_t alcatk : 4;
} s_alc_control_3;

typedef struct
{
	uint8_t alcnen : 1;
	uint8_t alcnth : 4;
} s_noise_gate;

typedef struct
{
	uint8_t pllmclk : 1;
	uint8_t plln : 4;
} s_pll_n;

typedef struct
{
	uint8_t pllk1 : 6;
} s_pll_k1;

typedef struct
{
	uint16_t pllk2 : 9;
} s_pll_k2;

typedef struct
{
	uint16_t pllk3 : 9;
} s_pll_k3;

typedef struct
{
	uint8_t depth3d : 4;
} s_depth_3d;

typedef struct
{
	uint8_t rmixmut : 1;
	uint8_t rsubbyp : 1;
	uint8_t rauxrsubg : 3;
	uint8_t rauxmut : 1;
} s_right_speaker_submixer;

typedef struct
{
	uint8_t micbiasv : 2;
	uint8_t rlinrpga : 1;
	uint8_t rmicnrpga : 1;
	uint8_t rmicprpga : 1;
	uint8_t llinlpga : 1;
	uint8_t lmicnlpga : 1;
	uint8_t lmicplpga : 1;
} s_input_control;

typedef struct
{
	uint8_t lpgau : 1;
	uint8_t lpgazc : 1;
	uint8_t lpgamt : 1;
	uint8_t lpgagain : 6;
} s_left_input_pga;

typedef struct
{
	uint8_t rpgau : 1;
	uint8_t rpgazc : 1;
	uint8_t rpgamt : 1;
	uint8_t rpgagain : 6;
} s_right_input_pga;

typedef struct
{
	uint8_t lpgabst : 1;
	uint8_t lpgabstgain : 3;
	uint8_t lauxbstgain : 3;
} s_left_adc_boost;

typedef struct
{
	uint8_t rpgabst : 1;
	uint8_t rpgabstgain : 3;
	uint8_t rauxbstgain : 3;
} s_right_adc_boost;

typedef struct
{
	uint8_t ldacrmx : 1;
	uint8_t rdaclmx : 1;
	uint8_t aux1bst : 1;
	uint8_t aux2bst : 1;
	uint8_t spkbst : 1;
	uint8_t tsen : 1;
	uint8_t aoutimp : 1;
} s_output_control;

typedef struct
{
	uint8_t lauxmxgain : 3;
	uint8_t lauxlmx : 1;
	uint8_t lbypmxgain : 3;
	uint8_t lbyplmx : 1;
	uint8_t ldaclmx : 1;
} s_left_mixer;

typedef struct
{
	uint8_t rauxmxgain : 3;
	uint8_t rauxrmx : 1;
	uint8_t rbypmxgain : 3;
	uint8_t rbyprmx : 1;
	uint8_t rdacrmx : 1;
} s_right_mixer;

typedef struct
{
	uint8_t lhpvu : 1;
	uint8_t lhpzc : 1;
	uint8_t lhpmute : 1;
	uint8_t lhpgain : 6;
} s_lhp_volume;

typedef struct
{
	uint8_t rhpvu : 1;
	uint8_t rhpzc : 1;
	uint8_t rhpmute : 1;
	uint8_t rhpgain : 6;
} s_rhp_volume;

typedef struct
{
	uint8_t lspkvu : 1;
	uint8_t lspkzc : 1;
	uint8_t lspkmute : 1;
	uint8_t lspkgain : 6;
} s_lspkput_volume;

typedef struct
{
	uint8_t rspkvu : 1;
	uint8_t rspkzc : 1;
	uint8_t rspkmute : 1;
	uint8_t rspkgain : 6;
} s_rspkput_volume;

typedef struct
{
	uint8_t auxout2mt : 1;
	uint8_t aux1mix2 : 1;
	uint8_t ladcaux2 : 1;
	uint8_t lmixaux2 : 1;
	uint8_t ldacaux2 : 1;
} s_aux_2_mixer;

typedef struct
{
	uint8_t auxout1mt : 1;
	uint8_t aux1half : 1;
	uint8_t lmixaux1 : 1;
	uint8_t ldacaux1 : 1;
	uint8_t radcaux1 : 1;
	uint8_t rmixaux1 : 1;
	uint8_t rdacaux1 : 1;
} s_aux_1_mixer;

typedef struct
{
	uint8_t lpdac : 1;
	uint8_t lpipbst : 1;
	uint8_t lpadc : 1;
	uint8_t lpspkd : 1;
	uint8_t micbiasm : 1;
	uint8_t regvolt : 2;
	uint8_t ibadj : 2;
} s_power_4;

typedef struct
{
	uint16_t left_slot : 9;
} s_left_time_slot;

typedef struct
{
	uint8_t pcmtsen : 1;
	uint8_t tri : 1;
	uint8_t pcm8bit : 1;
	uint8_t puden : 1;
	uint8_t pudpe : 1;
	uint8_t pudps : 1;
	uint8_t rtslot : 1;
	uint8_t ltslot : 1;
} s_misc;

typedef struct
{
	uint16_t right_slot : 9;
} s_right_time_slot;

typedef struct
{
	uint8_t mod_dither : 4;
	uint8_t analog_dither : 4;
} s_dac_dither;

typedef struct
{
	uint8_t alctblsel : 1;
	uint8_t alcpksel : 1;
	uint8_t alcngsel : 1;
	uint8_t alcgainl : 6; // ro
} s_alc_enhancement_1;

typedef struct
{
	uint8_t pklimena : 1;
	uint8_t alcgainr : 6; // ro
} s_alc_enhancement_2;

typedef struct
{
	uint8_t adcb_over : 1;
	uint8_t pll49mout : 1;
	uint8_t dac_osr32x : 1;
	uint8_t adc_osr32x : 1;
} s_sampling_192khz;

typedef struct
{
	uint8_t spiena_4w : 1;
	uint8_t fserrval : 2;
	uint8_t fserflsh : 1;
	uint8_t fserrena : 1;
	uint8_t notchdly : 1;
	uint8_t dacinmute : 1;
	uint8_t plllockbp : 1;
	uint8_t dacosr256 : 1;
} s_misc_controls;

typedef struct
{
	uint8_t maninena : 1;
	uint8_t manraux : 1;
	uint8_t manrlin : 1;
	uint8_t manrmicn : 1;
	uint8_t manrmicp : 1;
	uint8_t manlaux : 1;
	uint8_t manllin : 1;
	uint8_t manlmicn : 1;
	uint8_t manlmicp : 1;
} s_tieoff_1;

typedef struct
{
	uint8_t ibthalfi : 1;
	uint8_t ibt500up : 1;
	uint8_t ibt250dn : 1;
	uint8_t maninbbp : 1;
	uint8_t maninpad : 1;
	uint8_t manvrefh : 1;
	uint8_t manvrefm : 1;
	uint8_t manvrefl : 1;
} s_tieoff_2;

typedef struct
{
	uint8_t amutctrl : 1;
	uint8_t hvdet : 1;
	uint8_t nsgate : 1;
	uint8_t anamute : 1;
	uint8_t digmutel : 1;
	uint8_t digmuter : 1;
} s_automute_control;

typedef struct
{
	uint8_t manouten : 1;
	uint8_t shrtbufh : 1;
	uint8_t shrtbufl : 1;
	uint8_t shrtlspk : 1;
	uint8_t shrtrspk : 1;
	uint8_t shrtaux1 : 1;
	uint8_t shrtaux2 : 1;
	uint8_t shrtlhp : 1;
	uint8_t shrtrhp : 1;
} s_tieoff_3;

typedef struct
{
	uint16_t spi1 : 9;
} s_spi_1;

typedef struct
{
	uint16_t spi2 : 9;
} s_spi_2;

typedef struct
{
	uint16_t spi3 : 9;
} s_spi_3;

typedef struct
{
	s_power_1 power_1;
	s_power_2 power_2;
	s_power_3 power_3;
	s_audio_interface audio_interface;
	s_companding companding;
	s_clock_control_1 clock_contorl_1;
	s_clock_control_2 clock_control_2;
	s_gpio gpio;
	s_jack_detect_1 jack_detect_1;
	s_dac_control dac_control;
	s_left_dac_volume left_dac_volume;
	s_right_dac_volume right_dac_volume;
	s_jack_detect_2 jack_detect_2;
	s_adc_control adc_control;
	s_left_adc_volume left_adc_volume;
	s_right_adc_volume right_adc_volume;
	s_eq1 eq1;
	s_eq2 eq2;
	s_eq3 eq3;
	s_eq4 eq4;
	s_eq5 eq5;
	s_dac_limiter_1 dac_limiter_1;
	s_dac_limiter_2 dac_limiter_2;
	s_notch_filter_1 notch_filter_1;
	s_notch_filter_2 notch_filter_2;
	s_notch_filter_3 notch_filter_3;
	s_notch_filter_4 notch_filter_4;
	s_alc_control_1 alc_control_1;
	s_alc_control_2 alc_control_2;
	s_alc_control_3 alc_control_3;
	s_noise_gate noise_gate;
	s_pll_n pll_n;
	s_pll_k1 pll_k1;
	s_pll_k2 pll_k2;
	s_pll_k3 pll_k3;
	s_depth_3d depth_3d;
	s_right_speaker_submixer right_speaker_submixer;
	s_input_control input_control;
	s_left_input_pga left_input_pga;
	s_right_input_pga right_input_pga;
	s_left_adc_boost left_adc_boost;
	s_right_adc_boost right_adc_boost;
	s_output_control output_control;
	s_left_mixer left_mixer;
	s_right_mixer right_mixer;
	s_lhp_volume lhp_volume;
	s_rhp_volume rhp_volume;
	s_lspkput_volume lspkput_volume;
	s_rspkput_volume rspkput_volume;
	s_aux_2_mixer aux_2_mixer;
	s_aux_1_mixer aux_1_mixer;
	s_power_4 power_4;
	s_left_time_slot left_time_slot;
	s_misc misc;
	s_right_time_slot right_time_slot;
	s_dac_dither dac_dither;
	s_alc_enhancement_1 alc_enhancement_1;
	s_alc_enhancement_2 alc_enhancement_2;
	s_sampling_192khz sampling_192khz;
	s_misc_controls misc_controls;
	s_tieoff_1 tie_of_1;
	s_tieoff_2 tie_of_2;
	s_automute_control automute_control;
	s_tieoff_3 tie_of_3;
	s_spi_1 spi_1;
	s_spi_2 spi_2;
	s_spi_3 spi_3;
} ts_nau8822;
/////////////////////////////////////////////////
#define POWER_MANAGMENT_1 (1) // REG 1 default 0x000
#define DCBUFEN (8)			  // Power control for internal tie-off buffer used in 1.5X boost conditions 0: unpowered 1:enabled
#define AUX1MXEN (7)		  // Power control for AUX1 MIXER supporting AUXOUT1 analog output 0:unpowered 1:enabled
#define AUX2MXEN (6)		  // Power control for AUX2 MIXER supporting AUXOUT2 analog output 0:unpowered 1:enabled
#define PLLEN (5)			  // Power control for internal PLL 0:unpowered 1:enabled
#define MICBIASEN (4)		  // Power control for microphone bias buffer amplifier (MICBIAS output, pin#32) 0:unpowered 1:enabled
#define ABIASEN (3)			  // Power control for internal analog bias buffers 0:unpowered 1:enabled
#define IOBUFEN (2)			  // Power control for internal tie-off buffer used in non-boost mode (-1.0x gain) conditions 0:unpowered 1:enabled
#define REFIMP (0)			  // Select impedance of reference string used to establish VREF for internal bias buffers  REFIMP_OFF/REFIMP_80k/REFIMP_300k/REFIMP_3k
#define REFIMP_OFF (0)
#define REFIMP_80k (1)
#define REFIMP_300k (2)
#define REFIMP_3k (3)

#define POWER_MANAGMENT_2 (2) // REG 2 default 0x000
#define RHPEN (8)			  // Right Headphone driver enable, RHP analog output, pin#29 0:HI-Z 1:enabled
#define LHPEN (7)			  // Left Headphone driver enabled, LHP analog output pin#30 0:HI-Z 1:enabled
#define SLEEP (6)			  // Sleep enable 0:normal operation 1:sleep mode
#define RBSTEN (5)			  // Right channel input mixer, RADC Mix/Boost stage power control 0:RADC boost off 1: RADC boost ON
#define LBSTEN (4)			  // Left channel input mixer, LADC Mix/Boost stage power control 0:LADC boost off 1:LADC boost ON
#define RPGAEN (3)			  // Right channel input programmable amplifier (PGA) power control	0:off 1:enabled
#define LPGAEN (2)			  // Left channel input programmable amplifier (PGA) power control 0:off 1:enabled
#define RADCEN (1)			  // Right channel analog-to-digital converter power control 0:off 1:enabled
#define LADCEN (0)			  // Left channel analog-to-digital converter power control 0:off 1:enabled

#define POWER_MANAGMENT_3 (3) // REG 3 default 0x000
#define AUXOUT1EN (8)		  // AUXOUT1 analog output power control, pin#21 0:off 1:enabled
#define AUXOUT2EN (7)		  // AUXOUT2 analog output power control, pin#22 0:off 1:enabled
#define LSPKEN (6)			  // LSPKOUT left speaker driver power control, pin#25	0:off 1:enabled
#define RSPKEN (5)			  // RSPKOUT left speaker driver power control, pin#23 0:off 1:enabled
#define RMIXEN (3)			  // Right main mixer power control, RMAIN MIXER internal stage 0:off 1:enabled
#define LMIXEN (2)			  // Left main mixer power control, LMAIN MIXER internal stage 0:off 1:enabled
#define RDACEN (1)			  // Right channel digital-to-analog converter, RDAC, power control 0:off 1:enabled
#define LDACEN (0)			  // Left channel digital-to-analog converter, LDAC, power control 0:off 1:enabled

#define AUDIO_INTERFACE (4) // REG 4 default 0x050
#define BCLKP (8)			// Bit clock phase inversion option for BCLK, pin#8 0:normal 1:inverted
#define LRP (7)				// Phase control for I2S audio data bus interface 0:normal 1:inverted
#define WLEN (5)			// Word length (24-bits default) of audio data stream WLEN_32/WLEN_24/WLEN_20/WLEN_16
#define WLEN_32 (3)
#define WLEN_24 (2)
#define WLEN_20 (1)
#define WLEN_16 (0)
#define AIFMT (3) // Audio interface data format (default setting is I2S) RIGHT_JUST/LEFT_JUST/I2S_STANDARD/PCMAB
#define RIGHT_JUST (0)
#define LEFT_JUST (1)
#define I2S_STANDARD (2)
#define PCMAB (3)
#define DACPHS (2) // DAC audio data left-right ordering	0:left DAC data in left phase of LRP 1:left DAC data in right phase of LRP (left-right reversed)
#define ADCPHS (1) // ADC audio data left-right ordering  0:left ADC data is output in left phase of LRP 1:left ADC data is output in right phase of LRP (left-right reversed)
#define MONO (0)   // Mono operation enable 0:stereo 1:mono of LEFT

#define COMPANDING (5) // REG 5 default 0x000
#define CMB8 (5)	   // 8-bit DAC companding mode 0:normal 1:8-bit operation for companding mode
#define DACCM (3)	   // DAC companding mode control DAC_COMPANDING_OFF/DAC_U_LAW_COMPANDING/DAC_A_LAW_COMPANDING
#define COMPANDING_OFF (0)
#define DAC_U_LAW_COMPANDING (2)
#define DAC_A_LAW_COMPANDING (3)
#define ADCCM (1) // ADC companding mode control ADC_COMPANDING_OFF/ADC_U_LAW_COMPANDING/ADC_A_LAW_COMPANDING
#define ADC_COMPANDING_OFF (0)
#define ADC_U_LAW_COMPANDING (2)
#define ADC_A_LAW_COMPANDING (3)
#define ADDAP (0) // DAC audio data input option to route directly to ADC data stream 0:normal operation 1:ADC output data stream routed to DAC input data path

#define CLOCK_CONTROL_1 (6) // REG 6 default 0x140
#define CLKM (8)			// master clock source selection control 0:MCLK 1: internal PLL oscillator
#define MCLKSEL (5)			// Scaling of master clock source for internal 256fs rate ( divide by 2 = default) MCK_DIV_1/MCK_DIV_1_5/MCK_DIV_2/MCK_DIV_3/MCK_DIV_4/MCK_DIV_6/MCK_DIV_8/MCK_DIV_12
#define MCK_DIV_1 (0)
#define MCK_DIV_1_5 (1)
#define MCK_DIV_2 (2)
#define MCK_DIV_3 (3)
#define MCK_DIV_4 (4)
#define MCK_DIV_6 (5)
#define MCK_DIV_8 (6)
#define MCK_DIV_12 (7)
#define BCLKSEL (2) // Scaling of output frequency at BCLK pin#8 when chip is in master mode BCLK_DIV_1/BCLK_DIV_2/BCLK_DIV_4/BCLK_DIV_8/BCLK_DIV_16/BCLK_DIV_32
#define BCLK_DIV_1 (0)
#define BCLK_DIV_2 (1)
#define BCLK_DIV_4 (2)
#define BCLK_DIV_8 (3)
#define BCLK_DIV_16 (4)
#define BCLK_DIV_32 (5)
#define CLKIOEN (0) // Enables chip master mode to drive FS and BCLK outputs 0:FS and BCLK are inputs 1:FS and BCLK are outputs

#define CLOCK_CONTROL_2 (7) // REG 7 default 0x000
#define WSPIEN_4 (8)		// 4-wire control interface enable
#define SMPLR (1)			// Audio data sample rate indication (48kHz default). SAMPLE_RATE_48KHZ/SAMPLE_RATE_32KHZ/SAMPLE_RATE_24KHZ/SAMPLE_RATE_16KHZ/SAMPLE_RATE_8
#define FILTER_SAMPLE_RATE_48KHZ (0)
#define FILTER_SAMPLE_RATE_32KHZ (1)
#define FILTER_SAMPLE_RATE_24KHZ (2)
#define FILTER_SAMPLE_RATE_16KHZ (3)
#define FILTER_SAMPLE_RATE_8KHZ (5)
#define SCLKEN (0) // Slow timer clock enable. Starts internal timer clock derived by dividing master clock. 0:disabled 1:enabled

#define NAU_GPIO (8) // REG 8 default 0x000
#define GPIO1PLL (4) // Clock divisor applied to PLL clock for output from a GPIO pin GPIOPLL_DIV_1/GPIOPLL_DIV_2/GPIOPLL_DIV_3/GPIOPLL_DIV_4
#define GPIOPLL_DIV_1 (0)
#define GPIOPLL_DIV_2 (1)
#define GPIOPLL_DIV_3 (2)
#define GPIOPLL_DIV_4 (3)
#define GPIO1PL (3)					  // GPIO1 polarity inversion control 0:normal 1:inverted
#define GPIO1SEL (0)				  // CSB/GPIO1 function select (input default) GPIO1_INPUT/GPIO1_TEMP_OK/GPIO1_DAC_AUTOMUTE_STATUS/GPIO1_OUT_PLL/GIPO
#define GPIO1_INPUT (0)				  // use as input subject to MODE pin#18 input logic level
#define GPIO1_TEMP_OK (2)			  // Temperature OK status output ( logic 0 = thermal shutdown)
#define GPIO1_DAC_AUTOMUTE_STATUS (3) // DAC automute condition (logic 1 = one or both DACs automuted)
#define GPIO1_OUT_PLL (4)			  // output divided PLL clock
#define GIPO1_PLL_LOCK_STATUS (5)	  // PLL locked condition (logic 1 = PLL locked)
#define GPIO1_SET_OUTPUT_HIGH (6)	  // output set to logic 1 condition
#define GPIO1_SET_OUTPUT_LOW (7)	  // output set to logic 0 condition

#define JACK_DETECT_1 (9) // REG 9 default 0x000
#define JCKMIDEN (7)	  // Automatically enable internal bias amplifiers on jack detection state as sensed through GPIO pin associated to jack detection function bit7:logic 0 bit8:logic 1
#define JACDEN (6)		  // Jack detection feature enable 0:disabled 1:enabled
#define JCKDIO (4)		  // Select jack detect pin (GPIO1 default) 00:GPIO1 01:GPIO2 10:GPIO3 11:RESERVED

#define DAC_CONTROL (10) // REG 10 default 0x000
#define SOFTMT (6)		 // Softmute feature control for DACs 0:disabled 1:enabled
#define DACOS (3)		 // DAC oversampling rate selection (64X default) 0:64x 1:128x
#define AUTOMT (2)		 // DAC automute function enable	0:disabled 1:enabled
#define RDACPL (1)		 // DAC right channel output polarity control 0:normal 1:inverted
#define LDACPL (0)		 // DAC left channel output polarity control 0:normal 1:inverted

#define LEFT_DAC_VOLUME (11) // REG 11 default 0x0FF
#define LDACVU (8)			 // Write-only bit for synchronized L/R DAC changes
#define LDACGAIN (0)		 // DAC left digital volume control (0dB default attenuation value). Expressed as an attenuation value in 0.5dB steps, 0 for mute

#define RIGHT_DAC_VOLUME (12) // REG 12 default 0x0FF
#define RDACVU (8)			  // Write-only bit for synchronized L/R DAC changes
#define RDACGAIN (0)		  // DAC right digital volume control (0dB default attenuation value). Expressed as an attenuation value in 0.5dB steps, 0 for mute

#define JACK_DETECT_2 (13) // REG 13 default 0x000
#define JCKDOEN1 (4)	   // JACK detection input 1 enable: bit4:LR Headphone bit5:LR Speaker bit6:AUXOUT2 bit7:AUXOUT1
#define JCKDOEN0 (0)	   // JACK detection input 0 enable: bit0:LR Headphone bit1:LR Speaker bit2:AUXOUT2 bit3:AUXOUT1

#define ADC_CONTROL (14) // REG 14 default 0x100
#define HPFEN (8)		 // High pass filter enable control for filter of ADC output data stream 0:disabled 1:enabled
#define HPFAM (7)		 // High pass filter mode selection 0: normal audio mode 1:application specific mode, variable 2nd order high pass filter
#define HPF (4)			 // Application specific mode cutoff frequency selection
#define ADCOS (3)		 // ADC oversampling rate selection (64X default) 0:64x 1:128x
#define RADCPL (1)		 // ADC right channel polarity control 0:normal 1:inverted
#define LADCPL (0)		 // ADC left channel polarity control 0:normal 1:inverted

#define LEFT_ADC_VOLUME (15) // REG 15 default 0x0FF
#define LADCVU (8)			 // Write-only bit for synchronized L/R ADC changes
#define LADCGAIN (0)		 // ADC left digital volume control (0dB default attenuation value). Expressed as an attenuation value in 0.5dB steps, 0 for mute

#define RIGHT_ADC_VOLUME (16) /// REG 16 default 0x0FF
#define RADCVU (8)			  // Write-only bit for synchronized L/R ADC changes
#define RADCGAIN (0)		  // ADC right digital volume control (0dB default attenuation value). Expressed as an attenuation value in 0.5dB steps, 0 for mute

#define EQ_1_LOW_CUTOFF (18) // REG 18 default 0x12C
#define EQM (8)				 // Equalizer and 3D audio processing block assignment. 0:from ADC 1:from DAC(default)
#define EQ1CF (5)			 // Equalizer band 1 low pass -3dB cut-off frequency selection 00:80Hz 01:105Hz(default) 10:135Hz 11:175Hz
#define EQ1GC (0)			 // EQ Band 1 digital gain control. Expressed as a gain or attenuation in 1dB steps 01100:0db(default) 00000:12db 11000:-12db

#define EQ_2_PEAK_1 (19) // REG 19 default 0x02C
#define EQ2BW (8)		 // Equalizer Band 2 bandwidth selection 0:Narrow band 1:Wide band
#define EQ2CF (5)		 // Equalizer Band 2 center frequency selection 00:230Hz 01:300Hz(default) 10:385Hz 11:500Hz
#define EQ2GC (0)		 // EQ Band 2 digital gain control. Expressed as a gain or attenuation in 1dB steps 01100:0db(default) 00000:12db 11000:-12db

#define EQ_3_PEAK_2 (20) // REG 20 default 0x02C
#define EQ3BW (8)		 // Equalizer Band 3 bandwidth selection 0:Narrow band 1:Wide band
#define EQ3CF (5)		 // Equalizer Band 3 center frequency selection 00:650Hz 01:850Hz(default) 10:1.1KHz 11:1.4KHz
#define EQ3GC (0)		 // EQ Band 3 digital gain control. Expressed as a gain or attenuation in 1dB steps 01100:0db(default) 00000:12db 11000:-12db

#define EQ_4_PEAK_3 (21) // REG 21 default 0x02C
#define EQ4BW (8)		 // Equalizer Band 4 bandwidth selection 0:Narrow band 1:Wide band
#define EQ4CF (5)		 // Equalizer Band 4 center frequency selection 00:1.8KHz 01:2.4KHz(default) 10:3.2KHz 11:4.1KHz
#define EQ4GC (0)		 // EQ Band 4 digital gain control. Expressed as a gain or attenuation in 1dB steps 01100:0db(default) 00000:12db 11000:-12db

#define EQ5_HIGH_CUTOFF (22) // REG 22 default 0x02C
#define EQ5CF (5)			 // Equalizer Band 5 high pass -3dB cut-off frequency selection 00:5.3KHz 01:6.9KHz(default) 10:9.0KHz 11:11.7KHz
#define EQ5GC (0)			 // EQ Band 5 digital gain control. Expressed as a gain or attenuation in 1dB steps 01100:0db(default) 00000:12db 11000:-12db

#define DAC_LIMITER_1 (24) // REG 24 default 0x032
#define DACLIMEN (8)	   // DAC digital limiter control bit 0:disabled 1:enabled
#define DACLIMDCY (4)	   // DAC limiter decay time. Proportional to actual DAC sample rate. Duration doubles with each binary bit value. Values given here are for 44.1kHz sample rate
#define DACLIMATK (0)	   // DAC limiter attack time. Proportional to actual DAC sample rate. Duration doubles with each binary bit value. Values given here are for 44.1kHz sample rate

#define DAC_LIMITER_2 (25) // REG 25 default 0x000
#define DACLIMTHL (4)	   // DAC limiter threshold in relation to full scale output level (0.0dB = full scale) 000:-1db 001:-2db 010:-3db 011:-4db 100:-5db 101-111:-6db
#define DACLIMBST (0)	   // DAC limiter maximum automatic gain boost in limiter mode. If R24 limiter mode is disabled, specified gain value will be applied in addition to other gain values in the signal path.

#define NOTCH_FILTER_1 (27) // REG 27 default 0x000
#define NFCU1 (8)			// Update bit feature for simultaneous change of all notch filter parameters.
#define NFCEN (7)			// Notch filter control bit 0:disabled 1:enabled
#define NFCA0H (0)			// Notch filter A0 coefficient most significant bits.

#define NOTCH_FILTER_2 (28) // REG 28 default 0x000
#define NFCU2 (8)			// Update bit feature for simultaneous change of all notch filter parameters.
#define NFCA0L (0)			// Notch filter A0 coefficient least significant bits.

#define NOTCH_FILTER_3 (29) // REG 29 default 0x000
#define NFCU3 (8)			// Update bit feature for simultaneous change of all notch filter parameters.
#define NFCA1H (0)			// Notch filter A1 coefficient most significant bits.

#define NOTCH_FILTER_4 (30) // REG 30 default 0x000
#define NFCU4 (8)			// Update bit feature for simultaneous change of all notch filter parameters.
#define NFCA1L (0)			// Notch filter A1 coefficient least significant bits.

#define ALC_CONTROL_1 (32) // REG 32 default 0x038
#define ALCEN (7)		   // Automatic Level Control function control bits ALCEN_DISABLE/ALCEN_RIGHT_EN/ALCEN_LEFT_EN/ALCEN_BOTH_EN
#define ALCEN_DISABLE (0)
#define ALCEN_RIGHT_EN (1)
#define ALCEN_LEFT_EN (2)
#define ALCEN_BOTH_EN (3)
#define ALCMXGAIN (3) // Set maximum gain limit for PGA volume setting changes under ALC control 111:+35.25db(default) 000:-6.75db
#define ALCMNGAIN (0) // Set minimum gain value limit for PGA volume setting changes under ALC control 000:-12db(default) 111:+30db

#define ALC_CONTROL_2 (33) // REG 33 default 0x00B
#define ALCHT (4)		   // Hold time before ALC automated gain increase 0000:0ms(default)
#define ALCSL (0)		   // ALC target level at ADC output 1011:-6db full scale(default)

#define ALC_CONTROL_3 (34) // REG 34 default 0x032
#define ALCM (8)		   // ALC mode control setting 0:normal 1: Limiter Mode
#define ALCDCY (4)		   // ALC decay time duration per step of gain change for gain increase of 0.75dB of PGA gain.
#define ALCATK (0)		   // ALC attack time duration per step of gain change for gain decrease of 0.75dB of PGA gain.

#define NOISE_GATE (35) // REG 35 default 0x010
#define ALCNEN (3)		// ALC noise gate function control bit 0:disabled 1:enabled
#define ALCNTH (0)		// ALC noise gate threshold level 000:-39db default 111:-81db

#define PLLN_N (36) // REG 36 default 0x008
#define PLLMCLK (4) // Control bit for divide by 2 pre-scale of MCLK path to PLL clock input 0:devide by 1 1:devide by 2
#define PLLN (0)	// Integer portion of PLL input/output frequency ratio divider. Default decimal value is 8.

#define PLL_K_1 (37) // REG 37 default 0x00C
#define PLL_K_2 (38) // REG 38 default 0x093
#define PLL_K_3 (39) // REG 39 default 0x0E9

#define CONTROL_3D (41) // REG 41 default 0x000
#define DEPTH_3D (0)	// 3D Stereo Enhancement effect depth control 0000 = 0.0% effect (disabled, default) 0001: 6.67% effect...1111: 100.0% effect

#define RIGHT_SPEAKER_SUBMIXER (43) // REG 43 default 0x000
#define RMIXMUT (5)					// Mutes the RMIX speaker signal gain stage output in the right speaker submixer 0:unmuted 1:muted
#define RSUBBYP (4)					// Right speaker submixer bypass control 0:directly connected to RMIX speaker 1: connected to submixer output
#define RAUXRSUBG (1)				// RAUXIN to Right Speaker Submixer input gain control 000:-15db(default) 111:+6db
#define RAUXMUT (0)					// RAUXIN to Right Speaker Submixer mute control 0:muted 1:unmuted

#define INPUT_CONTROL (44) // REG 44 default 0x033
#define MICBIASV (7)	   // Microphone bias voltage selection control.Values change slightly with R58 MICBIAS mode selection control. Open circuit voltage on MICBIAS pin#32 is shown as follows as a fraction of the VDDA pin#31 supply voltage.
#define RLINRPGA (6)	   // RLIN right line input path control to right PGA positive input 0:not coonect to PGA 1:connected to PGA
#define RMICNRPGA (5)	   // RMICN right microphone negative input to right PGA negative input path control 0:not coonect to PGA 1:connected to PGA
#define RMICPRPGA (4)	   // RMICP right microphone positive input to right PGA positive input enable 0:not coonect to PGA 1:connected to PGA
#define LLINLPGA (2)	   // LLIN right line input path control to left PGA positive input 0:not coonect to PGA 1:connected to PGA
#define LMICNLPGA (1)	   // LMICN left microphone negative input to left PGA negative input path control 0:not coonect to PGA 1:connected to PGA
#define LMICPLPGA (0)	   // LMICP left microphone positive input to left PGA positive input enable 0:not coonect to PGA 1:connected to PGA

#define LEFT_INPUT_PGA_GAIN (45) // REG 45 default 0x010
#define LPGAU (8)				 // PGA volume update bit feature. Write-only bit for synchronized L/R PGA changes
#define LPGAZC (7)				 // Left channel input zero cross detection enable bit 0:gain changes to PGA register happen immediately (default) 1:gain changes to PGA happen pending zero crossing logic
#define LPGAMT (6)				 // Left channel mute PGA mute control.0: PGA not muted, normal operation (default) 1: PGA in muted condition not connected to LADC Mix/Boost stage
#define LPGAGAIN (0)			 // Left channel input PGA volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 010000:0db(default) 000000:-12db 111111:+35.25db

#define RIGHT_INPUT_PGA_GAIN (46) // REG 46 default 0x010
#define RPGAU (8)				  // PGA volume update bit feature. Write-only bit for synchronized L/R PGA changes
#define RPGAZC (7)				  // Right channel input zero cross detection enable bit 0:gain changes to PGA register happen immediately (default) 1:gain changes to PGA happen pending zero crossing logic
#define RPGAMT (6)				  // Right channel mute PGA mute control.0: PGA not muted, normal operation (default) 1: PGA in muted condition not connected to RADC Mix/Boost stage
#define RPGAGAIN (0)			  // Right channel input PGA volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 010000:0db(default) 000000:-12db 111111:+35.25db

#define LEFT_ADC_BOOST (47) // REG 47 default 0x100
#define LPGABST (8)			// Left channel PGA boost control 0:no gain 1:+20db gain
#define LPGABSTGAIN (4)		// Gain value between LLIN line input and LPGA Mix/Boost stage input 000:disconnected(default) 001:-12db 111:+6db
#define LAUXBSTGAIN (0)		// Gain value between LAUXIN auxiliary input and LPGA Mix/Boost stage input 000:disconnected(default) 001:-12db 111:+6db

#define RIGHT_ADC_BOOST (48) // REG 48 default 0x100
#define RPGABST (8)			 // Right channel PGA boost control 0:no gain 1:+20db gain
#define RPGABSTGAIN (4)		 // Gain value between RLIN line input and RPGA Mix/Boost stage input 000:disconnected(default) 001:-12db 111:+6db
#define RAUXBSTGAIN (0)		 // Gain value between RAUXIN auxiliary input and RPGA Mix/Boost stage input 000:disconnected(default) 001:-12db 111:+6db

#define OUTPUT_CONTROL (49) // REG 49 default 0x002
#define LDACRMX (6)			// Left DAC output to RMIX right output mixer cross-coupling path control 0:discoonect	1:connected
#define RDACLMX (5)			// Right DAC output to LMIX left output mixer cross-coupling path control 0:discoonect	1:connected
#define AUX1BST (4)			// AUXOUT1 gain boost control 0:<=3.6V 1:>3.6V
#define AUX2BST (3)			// AUXOUT2 gain boost control 0:<=3.6V 1:>3.6V
#define SPKBST (2)			// LSPKOUT and RSPKOUT speaker amplifier gain boost control 0:<=3.6V 1:>3.6V
#define TSEN (1)			// Thermal shutdown enable protects chip from thermal destruction on overload 0:disable 1:enable (default)
#define AOUTIMP (0)			// Output resistance control option for tie-off of unused or disabled outputs. Unused outputs tie to internal voltage reference for reduced pops and clicks. 0:1K default 1:30K

#define LEFT_MIXER (50) // REG 50 default 0x001
#define LAUXMXGAIN (6)	// Gain value between LAUXIN auxiliary input and input to LMAIN left output mixer 000:-15db 111:+6db
#define LAUXLMX (5)		// LAUXIN input to LMAIN left output mixer path control 0:not connected(default) 1:connected
#define LBYPMXGAIN (2)	// Gain value for bypass from LADC Mix/Boost output to LMAIN left output mixer. 000:-15db 111:+6db
#define LBYPLMX (1)		// Left bypass path control from LADC Mix/Boost output to LMAIN left output mixer 0:not connected 1:connected
#define LDACLMX (0)		// Left DAC output to LMIX left output mixer path control 0:not connected 1:connected

#define RIGHT_MIXER (51) // REG 51 default 0x001
#define RAUXMXGAIN (6)	 // Gain value between RAUXIN auxiliary input and input to RMAIN right output mixer 000:-15db 111:+6db
#define RAUXRMX (5)		 // RAUXIN input to RMAIN right output mixer path control 0:not connected(default) 1:connected
#define RBYPMXGAIN (2)	 // Gain value for bypass from RADC Mix/Boost output to RMAIN left output mixer. 000:-15db 111:+6db
#define RBYPRMX (1)		 // Right bypass path control from RADC Mix/Boost output to RMAIN r output mixer 0:not connected 1:connected
#define RDACRMX (0)		 // Right DAC output to RMIX right output mixer path control 0:not connected 1:connected

#define LHP_VOLUME (52) // REG 52 default 0x039
#define LHPVU (8)		// Write-only bit for synchronized changes of left and right headphone amplifier output settings
#define LHPZC (7)		// Left channel input zero cross detection enable 0:gain changes to  left headphone happen immediately(default) 1:gain changes to  left headphone happen pending zero crossing logic
#define LHPMUTE (6)		// Left headphone output mute control 0:normal operation(default) 1:muted
#define LHPGAIN (0)		// Left channel headphone output volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 111001:0db(default) 000000:-57db 111111:+6db

#define RHP_VOLUME (53) // REG 53 default 0x039
#define RHPVU (8)		// Write-only bit for synchronized changes of left and right headphone amplifier output settings
#define RHPZC (7)		// Right channel input zero cross detection enable 0:gain changes to  right headphone happen immediately(default) 1:gain changes to  right headphone happen pending zero crossing logic
#define RHPMUTE (6)		// Right headphone output mute control 0:normal operation(default) 1:muted
#define RHPGAIN (0)		// Right channel headphone output volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 111001:0db(default) 000000:-57db 111111:+6db

#define LSPKOUT_VOLUME (54) /// REG 54 default 0x039
#define LSPKVU (8)			// Write-only bit for synchronized changes of left and right speaker amplifier output settings
#define LSPKZC (7)			// Left channel input zero cross detection enable 0:gain changes to  left speaker happen immediately(default) 1:gain changes to  left speaker happen pending zero crossing logic
#define LSPKMUTE (6)		// Left speaker output mute control 0:normal operation(default) 1:muted
#define LSPKGAIN (0)		// Left channel speaker output volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 111001:0db(default) 000000:-57db 111111:+6db

#define RSPKOUT_VOLUME (55) // REG 55 default 0x039
#define RSPKVU (8)			// Write-only bit for synchronized changes of left and right speaker amplifier output settings
#define RSPKZC (7)			// Right channel input zero cross detection enable 0:gain changes to  right speaker happen immediately(default) 1:gain changes to  right speaker happen pending zero crossing logic
#define RSPKMUTE (6)		// Right speaker output mute control 0:normal operation(default) 1:muted
#define RSPKGAIN (0)		// Right channel speaker output volume control setting. Setting becomes active when allowed by zero crossing and/or update bit features. 111001:0db(default) 000000:-57db 111111:+6db

#define AUX2MIXER (56) // REG 56 default 0x001
#define AUXOUT2MT (6)  // AUXOUT2 output mute control 0:normal operation(default) 1:muted
#define AUX1MIX2 (3)   // AUX1 Mixer output to AUX2 MIXER input path control 0:not connected 1:connected
#define LADCAUX2 (2)   // Left LADC Mix/Boost output LINMIX path control to AUX2 MIXER input 0:not connected 1:connected
#define LMIXAUX2 (1)   // Left LMAIN MIXER output to AUX2 MIXER input path control 0:not connected 1:connected
#define LDACAUX2 (0)   // Left DAC output to AUX2 MIXER input path control 0:not connected 1:connected

#define AUX1MIXER (57) // REG 57 default 0x001
#define AUXOUT1MT (6)  // AUXOUT1 output mute control 0:normal operation(default) 1:muted
#define AUX1HALF (5)   // AUXOUT1 6dB attenuation enable 0:normal operation(default) 1:6dB attenuation
#define LMIXAUX1 (4)   // Left LMAIN MIXER output to AUX1 MIXER input path control 0:not connected 1:connected
#define LDACAUX1 (3)   // Left DAC output to AUX1 MIXER input path control 0:not connected 1:connected
#define RADCAUX1 (2)   // Right RADC Mix/Boost output RINMIX path control to AUX1 MIXER input 0:not connected 1:connected
#define RMIXAUX1 (1)   // Right RMIX output to AUX1 MIXER input path control 0:not connected 1:connected
#define RDACAUX1 (0)   // Right DAC output to AUX1 MIXER input path control 0:not connected 1:connected

#define POWER_MANAGMENT_4 (58) // REG 58 default 0x000
#define LPDAC (8)			   // Reduce DAC supply current 50% in low power operating mode 0:normal operation(default) 1:50% current mode
#define LPIPBST (7)			   // Reduce ADC Mix/Boost amplifier supply current 50% in low power operating mode 0:normal operation(default) 1:50% current mode
#define LPADC (6)			   // Reduce ADC supply current 50% in low power operating mode 0:normal operation(default) 1:50% current mode
#define LPSPKD (5)			   // Reduce loudspeaker amplifier supply current 50% in low power operating mode 0:normal operation(default) 1:50% current mode
#define MICBIASM (4)		   // Microphone bias optional low noise mode configuration control 0:normal operation(default) 1:low noise mode
#define REGVOLT (2)			   // Regulator voltage control power reduction options 00:normal 1.8V operation(default) 01:1.61V 10:1.40V 11:1.218V
#define IBADJ (0)			   // Master bias current power reduction options 00:normal 01:25% reduced 10:14% reduced 11: 25% reduced

#define LEFT_TIME_SLOT (59) // REG 59 default 0x000 Left channel PCM time slot start count: LSB portion of total number of bit times to wait from frame sync before clocking audio channel data. LSB portion is combined with MSB from R60 to get total number of bit times to wait.

#define MISC (60)	// REG 60 default 0x020
#define PCMTSEN (8) // Time slot function enable for PCM mode. 0:disabled 1:enabled
#define TRI (7)		// Tri state ADC out after second half of LSB enable 0:disabled 1:enabled
#define PCM8BIT (6) // PCM 8-bit word length enable 0:disable 1:enabled
#define PUDEN (5)	// ADCOUT output driver 1:enabled(default) 0:disabled
#define PUDPE (4)	// ADCOUT passive resistor pull-up or passive pull-down enable 0:disabled 1:enabled
#define PUDPS (3)	// ADCOUT passive resistor pull-up or pull-down selection 0:pull-down 1:pull-up
#define RTSLOT (1)	// Right channel PCM time slot start count: MSB portion of total number of bit times to wait from frame sync before clocking audio channel data. MSB is combined with LSB portion from R61 to get total number of bit times to wait.
#define LTSLOT (0)	// Left channel PCM time slot start count: MSB portion of total number of bit times to wait from frame sync before clocking audio channel data. MSB is combined with LSB portion from R59 to get total number of bit times to wait.

#define RIGHT_TIME_SLOT (61) // REG 61 default 0x000 Right channel PCM time slot start count: LSB portion of total number of bit times to wait from frame sync before clocking audio channel data. LSB portion is combined with MSB from R60 to get total number of bit times to wait.

#define DEVICE_REVISION_NUMBER (62) // Device Revision Number for readback over control interface = read-only value 0x07F for RevA silicon
#define DEVICE_ID (63)				// 0x01A Device ID equivalent to control bus address = read-only value

#define DAC_DITHER (65)	  // REG 65 default 0x114
#define MOD_DITHER (4)	  // Dither added to DAC modulator to eliminate all non-random noise 00000: dither off 10001: nominal optimal dither 11111: maximum dither
#define ANALOG_DITHER (0) // Dither added to DAC analog output to eliminate all non-random noise 0000: dither off 0100: nominal optimal dither 1111: maximum dither

#define ALC_ENHANCEMENT_1 (70) // REG 70 default 0x000
#define ALCTBLSEL (8)		   // Selects one of two tables used to set the target level for the ALC 0:default recommended target level table spanning -1.5dB through -22.5dB FS 1: optional ALC target level table spanning -6.0dB through -28.5dB FS
#define ALCPKSEL (7)		   // Choose peak or peak-to-peak value for ALC threshold logic 0:peak detector value 1:peak-to-peak detector value
#define ALCNGSEL (6)		   // Choose peak or peak-to-peak value for Noise Gate threshold logic 0:peak detector value 1:peak-to-peak detector value
#define ALCGAINL (0)		   // Real time readout of instantaneous gain value used by left channel PGA

#define ALC_ENHANCEMENT_2 (71) // REG 71 default 0x000
#define PKLIMENA (8)		   // Enable control for ALC fast peak limiter function 0:enabled 1:disabled
#define ALCGAINR (0)		   // Real time readout of instantaneous gain value used by right channel PGA

#define SAMPLING_192KHZ (72) // REG 72 default 0x008
#define ADCB_OVER (4)		 // ADC_Bias Current Override bit 0:100% Bias current for 48kHz Sampling 1:100% Bias current for 96kHz and 192kHz
#define PLL49MOUT (2)		 // Enable 49MHz PLL output 0: Divide by 4 block enabled 1: Divide by 2 block enabled
#define DAC_OSR32x (1)		 // Enable DAC_OSR32x 0:disabled 1:enabled
#define ADC_OSR32x (0)		 // Enable ADC_OSR32x 0:disabled 1:enabled

#define MISC_CONTROLS (73) // REG 73 default 0x000
#define SPIENA_4W (8)	   // Set SPI control bus mode regardless of state of Mode pin 0:normal operation 1:force 4-wire mode
#define FSERRVAL (6)	   // Short frame sync detection period value 00:trigger if frame time less than 255 MCLK edges 01:253 02: 254 03: 255
#define FSERFLSH (5)	   // Enable DSP state flush on short frame sync event 0:ignore short frame sync events (default) 1:set DSP state to initial conditions on short frame sync event
#define FSERRENA (4)	   // Enable control for short frame cycle detection logic 0:enabled 1:disabled
#define NOTCHDLY (3)	   // Enable control to delay use of notch filter output when filter is enabled 0:delay using notch filter output 512 sample times after notch enabled (default) 1:notch filter output used immediately
#define DACINMUTE (2)	   // Enable control to mute DAC limiter output when softmute is enabled 0:DAC limiter output may not move to exactly zero during Softmute (default) 1:DAC limiter output muted to exactly zero during softmute
#define PLLLOCKBP (1)	   // Enable control to use PLL output when PLL is not in phase locked condition 0:PLL VCO output disabled when PLL is in unlocked condition (default) 1: PLL VCO output used as-is when PLL is in unlocked condition
#define DACOSR256 (0)	   // Set DAC to use 256x oversampling rate (best at lower sample rates) 0: Use oversampling rate as determined by Register 0x0A[3] (default) 1:Set DAC to 256x oversampling rate regardless of Register 0x0A[3]

#define INPUT_TIE_OFF (74) // REG 74 default 0x000
#define MANINENA (8)	   // Enable direct control over input tie-off resistor switching 0:disabled 1:enabled
#define MANRAUX (7)		   // If MANUINEN = 1, use this bit to control right aux input tie-off resistor switch 0:force open 1:force closed
#define MANRLIN (6)		   // If MANUINEN = 1, use this bit to control right line input tie-off resistor switch 0:force open 1:force closed
#define MANRMICN (5)	   // If MANUINEN = 1, use this bit to control right PGA inverting input tie-off resistor switch 0:force open 1:force closed
#define MANRMICP (4)	   // If MANUINEN =1, use this bit to control right PGA non-inverting input tie-off switch 0:force open 1:force closed
#define MANLAUX (3)		   // If MANUINEN = 1, use this bit to control left aux input tie-off resistor switch 0:force open 1:force closed
#define MANLLIN (2)		   // If MANUINEN = 1, use this bit to control left line input tie-off resistor switch 0:force open 1:force closed
#define MANLMICN (1)	   // If MANUINEN = 1, use this bit to control left PGA inverting input tie-off switch 0:force open 1:force closed
#define MANLMICP (0)	   // If MANUINEN = 1, use this bit to control left PGA non-inverting input tie-off switch 0:force open 1:force closed

#define POWER_REDUCTION (75) // REG 75 default 0x000
#define IBTHALFI (8)		 // Reduce bias current to left and right input MIX/BOOST stage 0:normal 1:reduced by 50%
#define IBT500UP (6)		 // Increase bias current to left and right input MIX/BOOST stage 0:normal 1:increased by 500mA
#define IBT250DN (5)		 // Decrease bias current to left and right input MIX/BOOST stage 0:normal 1:decreased by 250mA
#define MANINBBP (4)		 // Direct manual control to turn on bypass switch around input tie-off buffer amplifier 0:normal 1: bypass switch to ground in in closed position when input buffer amplifier is disabled
#define MANINPAD (3)		 // Direct manual control to turn on switch to ground at input tie-off buffer amp output 0:normal 1:switch to ground in the closed position
#define MANVREFH (2)		 // Direct manual control of switch for Vref 600k-ohm resistor to ground 0:switch to ground controlled by Register 0x01 setting 1:switch to ground in the closed positioin
#define MANVREFM (1)		 // Direct manual control for switch for Vref 160k-ohm resistor to ground 0:switch to ground controlled by Register 0x01 setting 1:switch to ground in the closed positioin
#define MANVREFL (0)		 // Direct manual control for switch for Vref 6k-ohm resistor to ground ny:switch to ground controlled by Register 0x01 setting 1:switch to ground in the closed position

#define AGC_PP_READOUT (76)	 // Read-only register which outputs the instantaneous value contained in the peak-to-peak amplitude register used by the ALC for signal level dependent logic. Value is highest of left or right input when both inputs are under ALC control.
#define AGC_PP_DETECTOR (77) // Read-only register which outputs the instantaneous value contained in the peak detector amplitude register used by the ALC for signal level dependent logic. Value is highest of left or right input when both inputs are under ALC control.

#define STATUS_READOUT (78) // REG 78 default 0x000
#define AMUTCTRL (5)		// Select observation point used by DAC output automute feature 0: automute operates on data at the input to the DAC digital attenuator (default) 1:automute operates on data at the DACIN input pin
#define HVDET (4)			// Read-only status bit of high voltage detection circuit monitoring VDDSPK voltage 0:<=4V 1:>4V
#define NSGATE (3)			// Read-only status bit of logic controlling the noise gate function 0: greater than the noise gate threshold 1:less than the noise gate threshold
#define ANAMUTE (2)			// Read-only status bit of analog mute function applied to DAC channels 0:not in the automute condition 1:in automute condition
#define DIGMUTEL (1)		// Read-only status bit of digital mute function of the left channel DAC 0: digital gain value is greater than zero 1:digital gain is zero
#define DIGMUTER (0)		// Read-only status bit of digital mute function of the right channel DAC 0: digital gain value is greater than zero 1:digital gain is zero

#define OUTPUT_TIE_OFF (79) // REG 79 default 0x000
#define MANOUTEN (8)		// Enable direct control over output tie-off resistor switching 0:disabled 1:enabled
#define SHRTBUFH (7)		// If MANUOUTEN = 1, use this bit to control bypass switch around 1.5x boosted output tie-off buffer amplifier 0:normal 1: bypass switch in closed position
#define SHRTBUFL (6)		// If MANUOUTEN = 1, use this bit to control bypass switch around 1.0x non-boosted output tie-off buffer amplifier 0:normal 1: bypass switch in closed position
#define SHRTLSPK (5)		// If MANUOUTEN = 1, use this bit to control left speaker output tie-off resistor switch 0:force open 1:force closed
#define SHRTRSPK (4)		// If MANUOUTEN = 1, use this bit to control right speaker output tie-off resistor switch 0:force open 1:force closed
#define SHRTAUX1 (3)		// If MANUOUTEN = 1, use this bit to control Auxout1 output tie-off resistor switch 0:force open 1:force closed
#define SHRTAUX2 (2)		// If MANUOUTEN = 1, use this bit to control Auxout2 output tie-off resistor switch 0:force open 1:force closed
#define SHRTLHP (1)			// If MANUOUTEN = 1, use this bit to control left headphone output tie-off resistor switch 0:force open 1:force closed
#define SHRTRHP (0)			// If MANUOUTEN = 1, use this bit to control right headphone output tie-off resistor switch 0:force open 1:force closed

#define SPI1_REGISTER (87)
#define SPI1_VAL (0x0115)

#define SPI2_REGISTER (108)
#define SPI2_VAL (0x003B)

#define SPI3_REGISTER (115)
#define SPI3_VAL (0x0129)

void nau8822_mute_all(void);
void nau8822_unmute_all();

void nau8822_power_up(void);
void nau8822_init();

uint16_t nau8822_gain(enum e_gains g);

uint16_t nau8822_mic_bias_voltage(enum e_mic_bias_levels mbl); // voltage is equal to vdda times setting factor
uint16_t nau8822_3d_enhancement(uint8_t level);				   // 0 - off, 15 - max;

uint16_t nau8822_equ_src(enum e_equ_src es);					  // adc or dac
uint16_t nau8822_equ_band_1(enum e_equ_band_1 eb1, int8_t level); // level <-12 .. 11>
uint16_t nau8822_equ_band_2(enum e_equ_band_2 eb2, int8_t level); // level <-12 .. 11>
uint16_t nau8822_equ_band_3(enum e_equ_band_3 eb3, int8_t level); // level <-12 .. 11>
uint16_t nau8822_equ_band_4(enum e_equ_band_4 eb4, int8_t level); // level <-12 .. 11>
uint16_t nau8822_equ_band_5(enum e_equ_band_5 eb5, int8_t level); // level <-12 .. 11>

// outputs volumes
uint16_t nau8822_headphone_volume(uint8_t left, uint8_t right); // level <0 .. 63>
uint16_t nau8822_speaker_volume(uint8_t left, uint8_t right);	// leve; <0 .. 63>

// analog inputs
uint16_t nau8822_left_in_mix_src(enum e_left_in_mix_srcs ms, uint8_t gain);	  // pga, lin or aux
uint16_t nau8822_left_pga_in_src(enum e_left_pga_src ms, uint8_t gain);		  // mic or lin - can be gained by alc
uint16_t nau8822_right_in_mix_src(enum e_right_in_mix_srcs ms, uint8_t gain); // pga, lin or aux
uint16_t nau8822_right_pga_in_src(enum e_right_pga_src ms, uint8_t gain);	  // mix or lin - can be gained by alc

// internal mixers
uint16_t nau8822_left_main_mix_src(enum e_left_main_mix_srcs ms, uint8_t gain);	  // ldac, rdac, aux, in_mix stage
uint16_t nau8822_right_main_mix_src(enum e_right_main_mix_srcs ms, uint8_t gain); // rdac, ldac, aux, in_mix stage
uint16_t nau8822_aux_1_mix_src(enum e_aux_1_mix_srcs ms);						  // rinmix, ldac, rdac, lmix, rmix
uint16_t nau8822_aux_2_mix_src(enum e_aux_2_mix_srcs ms);						  // aux1(!), linmix, ldac, lmix
uint16_t nau8822_rspk_submix_src(enum e_submix_srcs ms);						  // rauxin or rmix

// alc - automatic level control
// this stuff can automatic set up gain on mic input (pga) or lin pga input
// and hold the output stage signal on ~constant level (if sound source is resonably loud).
// but it is a bit hard to proper setup, even if you work with datasheet, you propably will spent
// here a more time. Anyway it is worth, output signal can be free of distorion and dynamic range
// is wider than you think. I tested this future with signal gen and oscilloscope.
// With input signal in range 15mV - 500mV output signal was constant on goal level about ~1Vp-p with
// small deviation about 50mV WITHOUT distorion - just dynamic change of input gain
// https://www.youtube.com/watch?v=RilggJd1_LY - this guy use alc in his mic. He is loud but never too
// in 3 word: cool awsome nice future that you have to buy now...
// lets back on ground
// because you have to know how it work and how to setup it in your enviroment
// i don't write here any high level api
// lets look at example

// but this function get your 4 register param and set it to proper register
// however you may do it yourself
void nau8822_set_alc(void);

uint16_t nau8822_set_power_1(s_power_1 *pw); // ok
uint16_t nau8822_set_power_2(s_power_2 *pw); // ok
uint16_t nau8822_set_power_3(s_power_3 *pw);
uint16_t nau8822_set_power_4(s_power_4 *pw);
uint16_t nau8822_set_audio_interface(s_audio_interface *ai);
uint16_t nau8822_set_companding(s_companding *c);
uint16_t nau8822_set_clock_control_1(s_clock_control_1 *c);
uint16_t nau8822_set_clock_control_2(s_clock_control_2 *c);
uint16_t nau8822_set_gpio(s_gpio *g);
uint16_t nau8822_set_jack_detect_1(s_jack_detect_1 *j);
uint16_t nau8822_set_dac_control(s_dac_control *d);
uint16_t nau8822_set_left_dac_vol(s_left_dac_volume *d);
uint16_t nau8822_set_right_dac_vol(s_right_dac_volume *d);
uint16_t nau8822_set_jack_detect_2(s_jack_detect_2 *j);
uint16_t nau8822_set_adc_control(s_adc_control *c);
uint16_t nau8822_set_left_adc_vol(s_left_adc_volume *c);
uint16_t nau8822_set_right_adc_vol(s_right_adc_volume *c);
uint16_t nau8822_set_eq1(s_eq1 *e);
uint16_t nau8822_set_eq2(s_eq2 *e);
uint16_t nau8822_set_eq3(s_eq3 *e);
uint16_t nau8822_set_eq4(s_eq4 *e);
uint16_t nau8822_set_eq5(s_eq5 *e);
uint16_t nau8822_set_dac_lim_1(s_dac_limiter_1 *d);
uint16_t nau8822_set_dac_lim_2(s_dac_limiter_2 *d);
uint16_t nau8822_set_notch_1(s_notch_filter_1 *n);
uint16_t nau8822_set_notch_2(s_notch_filter_2 *n);
uint16_t nau8822_set_notch_3(s_notch_filter_3 *n);
uint16_t nau8822_set_notch_4(s_notch_filter_4 *n);
uint16_t nau8822_set_alc_1(s_alc_control_1 *a);
uint16_t nau8822_set_alc_2(s_alc_control_2 *a);
uint16_t nau8822_set_alc_3(s_alc_control_3 *a);
uint16_t nau8822_set_noise_gate(s_noise_gate *n);
uint16_t nau8822_set_plln(s_pll_n *p);
uint16_t nau8822_set_pllk1(s_pll_k1 *k);
uint16_t nau8822_set_pllk2(s_pll_k2 *k);
uint16_t nau8822_set_pllk3(s_pll_k3 *k);
uint16_t nau8822_set_3d_depth(s_depth_3d *k);
uint16_t nau8822_set_right_speaker_submixer(s_right_speaker_submixer *s);
uint16_t nau8822_set_input_control(s_input_control *c);
uint16_t nau8822_set_left_pga(s_left_input_pga *p);
uint16_t nau8822_set_right_pga(s_right_input_pga *p);
uint16_t nau8822_set_left_adc_boost(s_left_adc_boost *b);
uint16_t nau8822_set_right_adc_boost(s_right_adc_boost *b);
uint16_t nau8822_set_output_control(s_output_control *c);
uint16_t nau8822_set_left_main_mixer(s_left_mixer *m);
uint16_t nau8822_set_right_main_mixer(s_right_mixer *m);
uint16_t nau8822_set_lhp_vol(s_lhp_volume *v);
uint16_t nau8822_set_rhp_vol(s_rhp_volume *v);
uint16_t nau8822_set_lspkout_vol(s_lspkput_volume *v);
uint16_t nau8822_set_rspkout_vol(s_rspkput_volume *v);
uint16_t nau8822_set_aux_2_mix(s_aux_2_mixer *m);
uint16_t nau8822_set_aux_1_mix(s_aux_1_mixer *m);
uint16_t nau8822_set_left_time_slot(s_left_time_slot *t);
uint16_t nau8822_set_right_time_slot(s_right_time_slot *t);
uint16_t nau8822_set_misc(s_misc *m);
uint16_t nau8822_set_alc_enh_1(s_alc_enhancement_1 *e);
uint16_t nau8822_set_alc_enh_2(s_alc_enhancement_2 *e);
uint16_t nau8822_set_oversampling(s_sampling_192khz *s);
uint16_t nau8822_set_misc_ctrl(s_misc_controls *m);
uint16_t nau8822_set_tieoff_1(s_tieoff_1 *c);
uint16_t nau8822_set_tieoff_2(s_tieoff_2 *c);
uint16_t nau8822_set_tieoff_3(s_tieoff_3 *c);
uint16_t nau8822_set_automute(s_automute_control *m);

void nau8822_register_write(uint8_t reg, uint16_t data);
uint16_t nau8822_register_read(uint8_t reg);

extern ts_nau8822 snau8822;

#endif