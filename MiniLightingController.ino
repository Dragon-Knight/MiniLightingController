// ATtiny13 core: https://github.com/MCUdude/MicroCore
// Core settings:
// 	ATtiny13 @ 9.6 MHz internal osc.
// 	Disable: SAFEMODE, ENABLE_MILLIS, ENABLE_MICROS, SETUP_ADC.
// 	Enable:  ADC_PRESCALER_DEFAULT, SETUP_PWM, PWM_PRESCALER_DEFAULT, PWM_FAST.


constexpr uint8_t PIN_TRIG = 3;					// Пин запроса у HC-SR04.
constexpr uint8_t PIN_ECHO = 4;					// Пин ответа от HC-SR04.
constexpr uint8_t PIN_LOAD = 0;					// Пин ключа нагрузки.
constexpr uint8_t PIN_SW = 1;					// Пин внешней кнопки.
constexpr uint8_t PIN_USER = 2;					// Пин пользователя, A1 или 2.

constexpr uint8_t DISTANCE_ACTUATION_MIN = 30;	// Минимальное расстояние включения светодиода, в см.
constexpr uint8_t DISTANCE_ACTUATION_MAX = 70;	// Максимальное расстояние включения светодиода, в см.
constexpr uint16_t TIMEOUT_TIME = 32000;		// Время тайм-аута опроса, когда препятствие не обнаружено, в микросекундах.
constexpr uint8_t FADEIN_TICK = 6;				// Шагов за тик при повышении яркости. Если включено TIME_ALIGNMENT, то тик = TIMEOUT_TIME, иначе зависит от расстояния до объекта.
constexpr uint8_t FADEOUT_TICK = 1;				// Шагов за тик при понижении яркости. Если включено TIME_ALIGNMENT, то тик = TIMEOUT_TIME, иначе зависит от расстояния до объекта.
#define TIME_ALIGNMENT							// Выравнивает время между опросами, но тратит 80 байт кода.
constexpr uint8_t NOISE_REDUCTION = 10;			// Кол-во успешных считываний (расстояние в указанном диапазоне) перед включением светодиода.
#define IF_ON_THEN_ON							// Если разкомментировано, то в случае начала включения свет будет включатся, даже если расстояние до объекта выйдет за допустимые границы.

enum STATE : uint8_t
{
	STATE_OFF,
	STATE_FADEOUT,
	STATE_FADEIN,
	STATE_ON
};
STATE PWMState = STATE_OFF;
int16_t PWMLevel = 0;
volatile bool ForceEnable = false;
uint8_t CountReadingsDistance = 0;


inline uint16_t GetDistance();
inline void OnSwitchChange();


void setup()
{
	pinMode(PIN_TRIG, OUTPUT);
	pinMode(PIN_ECHO, INPUT);
	pinMode(PIN_LOAD, OUTPUT);
	pinMode(PIN_SW, INPUT);
	pinMode(PIN_USER, OUTPUT);
	
	digitalWrite(PIN_USER, HIGH);
	
	attachInterrupt(digitalPinToInterrupt(PIN_SW), OnSwitchChange, CHANGE);
	sei();
	
	return;
}

void loop()
{
	uint16_t distance = (ForceEnable == false) ? GetDistance() : DISTANCE_ACTUATION_MIN;
	
	// Т.к. при принудительном включении задержки, создаваемой функцией GetDistance() нету, то моментально яркость возрастает до 255.
	if(ForceEnable == true)
	{
		delay(TIMEOUT_TIME / 1000);
	}
	
	if(distance >= DISTANCE_ACTUATION_MIN && distance <= DISTANCE_ACTUATION_MAX)
	{
		if(PWMState == STATE_OFF || PWMState == STATE_FADEOUT)
		{
			if(++CountReadingsDistance >= NOISE_REDUCTION)
			{
				PWMState = STATE_FADEIN;
				
				digitalWrite(PIN_USER, LOW);
			}
		}
	}
	else
	{
#ifdef IF_ON_THEN_ON
		if(PWMState == STATE_ON)
#else
		if(PWMState == STATE_ON || PWMState == STATE_FADEIN)
#endif
		{
			if(--CountReadingsDistance == 0)
			{
				PWMState = STATE_FADEOUT;
				
				digitalWrite(PIN_USER, HIGH);
			}
		}
		
		// Принудительно чистим переменную, если свет не должен включатся, но пролетела муха (CountReadingsDistance > 0)
		if(PWMState == STATE_OFF && CountReadingsDistance > 0)
		{
			CountReadingsDistance = 0;
		}
	}
	
	switch(PWMState)
	{
		case STATE_FADEIN:
		{
			if(PWMLevel < 255)
			{
				PWMLevel += (PWMLevel + FADEIN_TICK <= 255) ? FADEIN_TICK : 255 - PWMLevel;
			}
			else
			{
				PWMState = STATE_ON;
			}
			
			break;
		}
		case STATE_FADEOUT:
		{
			if(PWMLevel > 0)
			{
				PWMLevel -= (PWMLevel - FADEOUT_TICK >= 0) ? FADEOUT_TICK : PWMLevel;
			}
			else
			{
				PWMState = STATE_OFF;
			}
			
			break;
		}
		default:
		{
			break;
		}
	}
	
	uint8_t crt_val = ((uint32_t)PWMLevel * PWMLevel * PWMLevel + 130305) >> 16;
	crt_val = (crt_val == 1) ? 0 : crt_val;
	analogWrite(PIN_LOAD, crt_val);
	
	return;
}

// Получение расстояния в см.
inline uint16_t GetDistance()
{
	digitalWrite(PIN_TRIG, HIGH);
	delayMicroseconds(10);
	digitalWrite(PIN_TRIG, LOW);
	
#ifdef TIME_ALIGNMENT
	uint16_t p = pulseIn(PIN_ECHO, HIGH, (TIMEOUT_TIME / 16));
	if(p > 0)
	{
		delay((TIMEOUT_TIME - p) / 1000);
	}
	return p / 58;
#else
	return pulseIn(PIN_ECHO, HIGH, (TIMEOUT_TIME / 16)) / 58;
#endif
}

// Прерывание изменение состояния выключателя.
inline void OnSwitchChange()
{
	ForceEnable = (digitalRead(PIN_SW) == HIGH) ? false : true;
	
	return;
}
