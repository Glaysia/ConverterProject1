# AGENT

프로젝트 목표
- LLC 방식(주파수 변조 기반) DC/DC 컨버터를 위한 구동 PWM 신호를 STM32F103에서 안정적으로 생성한다.
- 초기 목표: 50% 고정 듀티, 주파수 90~110 kHz 범위에서 1 kHz 단위 변경(버튼으로 스텝 변경). 이후 ADC 피드백으로 주파수 폐루프 제어 확장.

현재 상태(펌웨어/하드웨어)
- MCU: STM32F103RB (NUCLEO-F103RB 기준)
- 클록: HSE BYPASS, PLL x9 → SYSCLK 72 MHz, APB1=DIV2, APB2=DIV1
- 타이머: TIM1 사용, CH1(메인) + CH1N(상보), 데드타임 사용
  - 핀: CH1=PA8, CH1N=PA7 (TIM1 부분 리매핑 적용)
  - 초기 설정: PSC=0, ARR=719(100 kHz), CCR1≈360(50%), DeadTime 코드값≈22(≈300 ns)
- 버튼: PC13(NUCLEO USER 버튼) → EXTI15_10으로 사용 예정(소프트 디바운싱 필요)
- 빌드: Makefile 기반, ARM GCC(`arm-none-eabi-gcc`)
- VS Code: `.vscode/c_cpp_properties.json` 구성 완료(HAL/CMSIS 경로 + 매크로 정의)

LLC 구동 신호 요구사항(초기)
- 위상: 하프브리지 구동을 위한 상보 PWM(메인/상보), 동일 주파수·동일 듀티(50%)
- 데드타임: 150~500 ns 수준에서 시작(하드웨어·드라이버 스펙 고려), 기본 200~300 ns 권장
- 주파수 제어: PFM(주파수 변조)만 사용, 듀티는 50% 고정
- 스텝 변경: 버튼 1회당 +1 kHz, 110 kHz 도달 시 90 kHz로 롤오버(또는 반대)

CubeMX 설정 가이드(요약)
- RCC: HSE BYPASS, PLL Source=HSE, PLL MUL=9, Flash Latency=2
- TIM1:
  - Channel: PWM Generation CH1 + Complementary Output(CH1N)
  - Prescaler=0, Period(ARR)=round(72e6/f)−1, Pulse(CCR1)=(ARR+1)/2
  - OC Mode=PWM1, Polarity=High/High, OC Fast Mode=Disable
  - Preload: ARR Preload Enable, OC Preload Enable(런타임 갱신의 글리치 최소화)
  - Break & Dead‑Time: Break=Disable(기본), DeadTime=코드값(≈14~22부터)
- GPIO: PA8/PA7 = AF Push‑Pull, Speed=High(100 kHz 여유 확보)
- EXTI: PC13 Input Pull‑up + Falling edge, NVIC `EXTI15_10` Enable

펌웨어 설계(핵심 동작)
- 초기화: `MX_TIM1_Init()` 이후
  - `HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);`
  - `HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);`
- 주파수 변경(런타임): ARR/CCR1 갱신 후 업데이트 이벤트로 래치
  - ARR = 72e6 / f_target − 1
  - CCR1 = (ARR+1)/2  // 50%
  - `__HAL_TIM_SET_AUTORELOAD(&htim1, ARR);`
  - `__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, CCR1);`
  - `__HAL_TIM_GENERATE_EVENT(&htim1, TIM_EVENTSOURCE_UPDATE);`
- 버튼 ISR(개요):
  - 디바운스(예: 5~20 ms) 후 `f_target += 1 kHz` (110 kHz 초과 시 90 kHz로 래핑)
  - 위 수식으로 ARR/CCR 계산·적용

안전/하드웨어 유의사항
- 상보 출력은 자동 반상태 + 데드타임 삽입. CH/CHN Polarity는 High/High로 시작(외부 드라이버가 Active‑Low면 해당 신호만 반전).
- DeadTime 코딩(BDTR.DTG) 규칙 숙지. 72 MHz, CKD=DIV1에서는 127 이하 코드값은 선형(약 13.9 ns/코드).
- Break(BKIN)를 쓰지 않을 경우 반드시 Disable. 배선 없이 Enable 시 출력 차단 가능.
- 실제 전력 스테이지 구동 전, 오실로스코프로 무부하·모형부하에서 교차도통(데드타임 불충분) 여부 검증.

검증 체크리스트
- [ ] PA8/PA7에서 50% 듀티 상보 파형 확인
- [ ] 주파수: 90↔110 kHz 범위 1 kHz 스텝 변경 시 글리치/스킵 없음
- [ ] 데드타임: 전환부에서 평평한 구간 확인(설정값과 일치)
- [ ] 버튼 디바운스 유효, 과도 트리거 없음
- [ ] Break 비활성 확인, BKIN 노이즈로 인한 차단 없음

로드맵(우선순위)
1) 버튼 기반 주파수 스텝 변경(90~110 kHz, 1 kHz step) 구현 + 디바운싱
2) ARR/CCR 갱신 시 프리로드/UG 이벤트로 글리치 최소화, 측정/튜닝
3) DeadTime 최적화(드라이버/소자 t_rise/t_fall 기반, 여유 포함)
4) UART 로깅(현재 f_target, ARR/CCR, dt 코드값)
5) ADC 도입: VIN/VOUT/IOUT 샘플링(우선 폴링 또는 DMA, 타이밍 정합)
6) 폐루프: 출력전압(또는 기초 전류/주파수 맵) 기반 주파수 제어(안정성 검토)
7) 보호: OCP/OVP/OTP, BKIN/FAULT 연동, 소프트스타트(주파수 램프)

코딩 컨벤션/참조
- HAL API 우선 사용, 레지스터 매크로로 성능 보완(ARR/CCR 갱신 등)
- 인터럽트 핸들러: 처리 최소화(계산은 main 루프/타이머 주기 작업으로 분담 가능)
- 관련 파일
  - `Core/Src/main.c`(타이머/클록/스타트): TIM1 설정과 시작 호출
  - `Core/Src/stm32f1xx_hal_msp.c`(핀/리매핑): PA8/PA7 AF_PP, TIM1 부분 리맵
  - `Drivers/STM32F1xx_HAL_Driver/...` HAL 드라이버 일체
  - `.vscode/c_cpp_properties.json` IntelliSense/컴파일러 경로·매크로

빌드/실행
- Makefile 빌드: `make` (툴체인 경로가 환경에 맞는지 확인)
- 플래시/디버깅: ST‑Link(예: `st-flash`, `openocd`/`gdb`) 환경에 맞춰 사용

메모
- LLC 제어 특성상 듀티는 50% 유지가 기본. 향후 특수 상황에서만 변조 고려.
- TIM1은 Advanced Timer로 보조 출력/데드타임/브레이크 등 기능 포함. 필요 시 TIM1 CH2/CH2N까지 확장 가능(풀브리지 등).

