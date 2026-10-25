# CLAUDE.md — firmware sailoQ remote

Contexto para Claude en VS Code. **Léelo entero antes de tocar código.**

## Contexto general: la gama sailoQ
sailoQ (antes nautiQ, www.sailoq.com) es el proyecto propio de **José Piquer**: electrónica para veleros que se vende en pequeñas series, llave en mano y con soporte. José navega en el Mediterráneo y prueba todo en su propio barco.

| Producto | Qué es | Carpeta (en `sailoQ-global/`) |
|---|---|---|
| **lite STM32** (cube-lite, v1.5 con GPS / v1.6 sin GPS) | Emulador de corredera desde el GPS, pasarela NMEA 0183 ⇄ SeaTalk 1, mando de 433 MHz para el piloto y radio nRF24. | `sailoQ lite stm32` |
| **ITF3** | Interfaz tipo Raymarine iTC‑5: lee veleta, corredera y sonda y lo saca por SeaTalk, NMEA 2000 y NMEA 0183. | `sailoQ ITF3` |
| **pro** | Modelo más completo de la gama; en fase inicial. | `sailoQ pro` |
| **remote** (este) | Mando a distancia con pantalla para el piloto, el molinete y la hélice de proa, y para ver datos del barco. | `sailoQ remote` |
| **Receptor de molinete y hélice** (B8) | Placa aún por diseñar, con 4 contactores. | — |

Así se reparte el trabajo en el sistema:
- **La base** (lite o pro) está conectada a los buses del barco. Hace de puente por radio entre esos buses y el mando: le da los datos y mueve el piloto con datagramas SeaTalk 0x86.
- **El actuador** (receptor de molinete y hélice) mueve los contactores mientras el mando mantiene la orden ("hombre muerto").

**El protocolo entre los tres equipos y su código están en la librería `sailoQ_radio`** (carpeta hermana `../sailoQ_radio`, enlazada en `platformio.ini` con `lib_deps = symlink://../sailoQ_radio`):
- `PROTOCOLO_RADIO.md` y `include/sq_proto.h`: el contrato;
- `sq_mando.c` (lo usa este mando) y `sq_receptor.c` (lo usarán la base y el actuador);
- tiene su propio `CLAUDE.md` con sus reglas y su lista de tareas (R1…). Si cambias algo allí, pasa su prueba (`sh test/host/probar.sh` dentro de la librería).

### Cómo trabajar con José
- Es aficionado a la electrónica, no experto, y viene de Arduino. **Escríbele en español** y explica las cosas sin dar por supuesto mucho conocimiento previo.
- Cuando comenta alternativas las está **valorando, no decidiendo**: trátalas como datos para la valoración.
- Quiere **listas de tareas** con identificadores únicos, sin filas obsoletas y dejando claro qué está resuelto y qué no.
- Prefiere **componentes y huellas de las librerías estándar** de EasyEDA (los que suelda él, marcados "sin BOM").
- **Carpetas de solo lectura:** las de los otros productos (`sailoQ lite stm32`, `sailoQ ITF3`, `sailoQ pro`) se pueden consultar, pero **no se modifica nada allí**. Otra instancia de Claude lleva cada firmware. Si hace falta algo en la base (por ejemplo, implementar su lado del protocolo), se apunta como tarea y se le pasa a José.
- No compartir el firmware ni los ficheros del PCB fuera del proyecto.

## Hardware (esquema `SCH_sailoQ-remote_v9_Claude.json`, PCB `PCB_sailoQ-remote_v1.1_rutada_Claude.json`)
- **Caja:** unos 60 × 140 × 30 mm. Placa de 54 × 134 mm con la parte de abajo redondeada, 2 capas.
  - **Cara de arriba:** pantalla, teclas y electrónica.
  - **Cara de abajo:** portapilas 18650 (BT1) y módulo nRF24 (RF1), enchufado con la antena hacia el borde de arriba.
- **MCU:** STM32L071CBT6 (Cortex‑M0+, 128 KB de flash, 20 KB de RAM, 6 KB de EEPROM de datos).
  - Reloj: **HSI16 interno** (sin cristal principal) más **LSE de 32,768 kHz** para el RTC.
  - Regulador en rango 2.
- **Alimentación:**
  - USB‑C (o pogo J2 con D5) → TP4056 (carga a unos 545 mA, NTC RT1) → VBAT.
  - Protección DW01A + FS8205A. Q4 protege contra la pila al revés.
  - Power path con Q1 + D3 → VDD → LDO XC6206 → 3,3 V.
  - **Sin interruptor:** el micro duerme en modo Stop. Objetivo: menos de 20 µA en total.
- **Programación:** SWD con ST‑Link por **PROG** (franja derecha: 1 GND, 2 CLK, 3 DIO, 4 3V3, 5 RST). **No reconfigures PA13/PA14.**
  - Además, el cargador serie del STM32 está accesible por la tira **DBG** (franja izquierda: 1 GND, 2 3V3, 3 TX, 4 RX, 5 BOOT0, 6 VBAT).
- **Reset sin abrir la caja:** reed SW9 entre NRST y GND (se acerca un imán), más el watchdog IWDG.

### Mapa de pines (detalle en `include/board.h`)
| Función | Pin | Periférico | Notas |
|---|---|---|---|
| nRF24 SCK / MISO / MOSI | PA5 / PA6 / PA7 | SPI1, AF0 | Modo 0, 8 MHz. |
| nRF24 CSN / CE / IRQ | PA4 / PB0 / PB1 | GPIO | IRQ se lee por sondeo. |
| Pantalla SCK / SDA | PB13 / PB15 | SPI2, AF0 | 4 MHz, solo escritura. |
| Pantalla CS / A0 / RST | PB12 / PB14 / PB2 | GPIO | A0: 0 = comando, 1 = dato. |
| Retroiluminación | PA15 | TIM2_CH1, AF5 | PWM de 1 kHz a Q3 (cátodo); R18 de 47 Ω desde VDD. |
| Zumbador | PA2 | TIM21_CH1, AF0 | 2,7 kHz al 50 %; Q5, D4. |
| Teclas PÁG◄ / PÁG► | PB3 / PB4 | GPIO + EXTI | Pull‑up interno; pulsada = 0. |
| Teclas K1…K6 | PB5…PB10 | GPIO + EXTI | 4 filas × 2: (PÁG◄ PÁG►) (K1 K2) (K3 K4) (K5 K6). |
| VBUS_DET | PA0 | ADC_IN0 | VBUS/2. |
| VBAT_ADC | PA1 | ADC_IN1 | VBAT/2 con divisor de 1 MΩ: muestreo de 160,5 ciclos. |
| Sensor de luz | PA3 (ADC_IN3) + PA8 LIGHT_EN | ADC + GPIO | Alimentado solo al medir. **Puede que la caja no tenga ventana:** por defecto, brillo manual. |
| Depuración | PA9 TX / PA10 RX | USART1, AF4 | 115200 8N1 en la tira DBG. |
| LSE | PC14 / PC15 | — | Cristal de 32,768 kHz y 12,5 pF. |

Pines libres: PA11, PA12, PB11, PC13, PH0 y PH1.

### Pantalla ERC128128FS02-3 (ST7571)
- 128 × 128, FSTN positivo con fondo blanco: se lee al sol. Hojas en `../datasheets/pantalla/`.
- Bus SPI de 4 hilos: PS0 = 0, PS1 = 1, PS2 = 0.
- **El ST7571 es de 4 grises:** cada columna de cada página ocupa **2 bytes**. En blanco y negro se manda el mismo byte dos veces (`st7571.c`).
- La pantalla no se puede leer: se dibuja en un *framebuffer* de 2 KB (`gfx.c`) y se vuelca entero (unos 10 ms).
- **Pendiente con la pantalla real (F2):**
  - orientación: `LCD_ADC_REV`, `LCD_SHL_REV` y `LCD_COM0` en `st7571.h`;
  - contraste (EV, por defecto 35, ajustable en el menú);
  - los comandos `0x67` y `0xF3 0x04` del ejemplo de Sitronix.
- La cinta (FPC) va doblada bajo la pantalla, con el conector de contacto inferior **0.5K‑AX‑31PWB**.
- Los píxeles miden 0,28 × 0,33 mm: las cifras grandes van estrechadas un 15 % (`tools/fuentes.py`).

## Software
- **Herramientas:** PlatformIO en VS Code, con `framework = stm32cube` (HAL de ST). **Sin CubeMX y sin CubeIDE.**
  - Reloj y periféricos escritos a mano en `src/periph.c`.
  - `include/stm32l0xx_hal_conf.h` solo activa los módulos que se usan.
  - La placa se define en `boards/sailoq_remote_l071cb.json`, porque PlatformIO no trae una para el L071CB. Si PlatformIO protesta por el script del enlazador, añade `board_build.ldscript` con uno de 128 KB de flash y 20 KB de RAM.
- **Repositorio:** `git@github.com:jopimo/sailoQ_remote.git` (privado, rama `main`, acceso por SSH). La librería de radio va en su propio repositorio (`jopimo/sailoQ_radio_nRF24L01`).
- **Compilar:** botón "Build". **Grabar:** botón "Upload" con ST‑Link. **Depurar:** F5. **Monitor serie:** 115200.
- **Estructura:**
  - `src/main.c`: arranque, bucle principal (teclas cada 10 ms), medidas cada 2 s, dormir/despertar y corte por batería baja.
  - `src/periph.c`: reloj, GPIO, SPI1/SPI2, TIM2/TIM21 (PWM), ADC (con VREFINT), USART1, RTC (despertador) e IWDG (unos 16 s).
  - `src/st7571.c`: pantalla. `src/gfx.c`: dibujo (líneas, círculos, texto 6×8, cifras grandes). `src/font6x8.c`: la misma fuente que la ITF3. `src/font_big.c`: generado con `tools/fuentes.py`.
  - `src/radio.c`: une la librería de radio (`sq_mando.h`) con los ajustes del mando (los emparejamientos van en `cfg`) y guarda los últimos datos recibidos (`nav`, `act`) para la interfaz.
  - `src/sq_port.c`: lo que la librería necesita de esta placa (SPI1, CSN, CE, IRQ, esperas, reloj y UID).
  - `src/keys.c`: antirrebote y eventos PRESS, LONG (600 ms) y RELEASE.
  - `src/ui.c`: páginas y lógica de teclas.
  - `src/power.c`: batería, carga, luz y modo Stop. `src/buzzer.c`: pitidos. `src/config.c`: ajustes en la EEPROM (emparejamientos, apagado, brillo, contraste, pitido y nº de campos).
  - `test/host/`: compila la interfaz en el PC y saca PNG de todas las pantallas (`sh test/host/probar.sh`). **Úsalo después de cambiar algo de `ui.c` o `gfx.c`** y mira las imágenes de `test/host/out/`.
- **Páginas:** solo se muestran las de lo que está emparejado.
  - Con base: DATOS (bidata o tridata), VIENTO y PILOTO.
  - Con actuador: MOLINETE y HÉLICE.
  - AJUSTES: manteniendo PÁG◄ + PÁG► 3 s. Sale sola en el primer arranque.
  - Textos sin acentos (la fuente es ASCII).
- **Teclas del piloto:**
  - K5 STBY actúa **al pulsar** (seguridad); K6 AUTO con pulsación larga.
  - K1 −1, K2 +1, K3 −10 y K4 +10 actúan al soltar.
  - Combinaciones (mantener 0,5 s): K1+K3 virada a babor, K2+K4 virada a estribor, K5+K6 modo viento.
- **Molinete y hélice:** K1/K2 solo mientras se mantienen. El mando repite la orden cada 100 ms y el actuador para solo a los 300 ms sin recibir nada.
- **Energía:**
  - Tras `cfg.apagado_s` sin teclas (60 s por defecto; los últimos 10 s con el brillo a un tercio) se apagan pantalla, retroiluminación y radio, y el micro entra en Stop.
  - El RTC lo despierta cada 8 s solo para refrescar el watchdog.
  - **La tecla que despierta no actúa.**
  - Batería por debajo de 3,30 V (3 medidas seguidas y sin USB): aviso y a dormir. Por debajo de 3,40 V, el icono de la batería parpadea.
- **Ocupación:** unos 33 KB de flash y 3,5 KB de RAM estática (PlatformIO, arm‑none‑eabi‑gcc 7.2.1, framework‑stm32cubel0 1.12.4, 2026‑10‑25). Sin avisos con `-Wall -Wextra` en nuestro código (solo salen dos de la HAL de ST).
- **Nada de este código se ha probado todavía en la placa.** Cada parte se valida en el prototipo, una a una: sigue `PUESTA_EN_MARCHA.md`.

## Pendiente (firmware)
Las tareas de radio comunes a todos los equipos están en la lista de la librería `sailoQ_radio` (R1–R5): subirla a GitHub, primera prueba con radios reales, seguridad del actuador e integración en la lite.

| Id | Tarea | Estado |
|---|---|---|
| F1 | Primera prueba en placa: reloj y LSE, teclas, zumbador, retroiluminación, ADC (VBAT/VBUS) y watchdog | pendiente (placa) |
| F2 | Pantalla: arranque del ST7571, orientación (ADC/SHL/COM0), contraste y legibilidad al sol | pendiente (placa) |
| F3 | Comprobar que el módulo es nRF24L01**+** (250 kbps): `sqn_is_plus()` de la librería lo dice. Si no, `PROTO_RF_SETUP = 0x06` (1 Mbps) en la librería | pendiente (placa) |
| F5 | **Actuador** (molinete/hélice, B8): placa y firmware nuevos, con hombre muerto de 300 ms, enclavamientos, tiempo máximo y contador de cadena. Su radio: `sq_receptor.h` de la librería | pendiente |
| F7 | Medir el consumo dormido (objetivo < 20 µA) y ajustar | pendiente (placa) |
| F8 | Curva de la batería y umbrales `VBAT_LOW_MV` / `VBAT_CUT_MV` con la 18650 real | pendiente |
| F9 | Sensor de luz: decidir si la caja lleva ventana. Si no, quitar la opción AUTO del brillo | pendiente (José) |
| F10 | Confirmar con el piloto real el origen del datagrama 0x86 (X=1, como en la lite) | pendiente (base) |
| F11 | Página DATOS: elegir qué campos van en bidata/tridata (hoy: velocidad STW/SOG, profundidad y AWS) | pendiente (José) |
| F12 | Usar los 4 grises del ST7571 (por ahora solo blanco y negro) | opcional |

## Archivos del proyecto (carpeta superior)
- `SCH_sailoQ-remote_v9_Claude.json`: esquema vigente (EasyEDA Std, 6 hojas).
- `PCB_sailoQ-remote_v1.1_rutada_Claude.json`: PCB vigente.
- `render/`: imágenes y vídeo para la web.
- `datasheets/`: STM32L071 y pantalla (ERC128128FS02‑3, ST7571, conectores).
