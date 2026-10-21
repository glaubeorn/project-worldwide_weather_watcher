#include "grove_light.h"
#include "board_pins.h"

ADC_HandleTypeDef hadc1;

#define LIGHT_FLOATING_DELTA 400 // Entrée en l'air mesurée : écart ~1270 ; capteur branché : écart quasi nul

void GroveLight_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = LIGHT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;

    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = LIGHT_ADC_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

/*
 * Lecture après avoir chargé l'entrée avec la résistance interne (pull-up ou pull-down).
 * Capteur branché : sa sortie (ampli LM358) impose la tension, la précharge n'a pas d'effet.
 * Entrée en l'air : la tension préchargée reste, la lecture suit la résistance choisie.
 */
static uint16_t read_with_precharge(uint32_t pull)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = LIGHT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = pull;
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);
    HAL_Delay(1);

    gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);

    return GroveLight_ReadRaw();
}

bool GroveLight_IsConnected(void)
{
    uint16_t low = read_with_precharge(GPIO_PULLDOWN);
    uint16_t high = read_with_precharge(GPIO_PULLUP);

    if (low == 0xFFFF || high == 0xFFFF)
        return false;

    // Écart important entre les deux précharges : entrée en l'air
    return (int)high - (int)low < LIGHT_FLOATING_DELTA;
}

uint16_t GroveLight_ReadRaw(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return 0xFFFF;

    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0xFFFF;
    }

    uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}