# Puesta en marcha del sailoQ remote (primer prototipo)

Guía paso a paso para cuando lleguen las placas. **Ve en orden.** Si un paso falla, no sigas: apunta lo que ves y pásaselo a Claude con el número del paso.

## Material
- ST-Link V2 conectado al conector **PROG**, en la franja derecha de la placa:
  - 1 GND
  - 2 SWCLK
  - 3 SWDIO
  - 4 3V3
  - 5 NRST
- Adaptador USB-serie de 3,3 V en la tira **DBG**, en la franja izquierda:
  - 1 GND
  - 3 TX del mando → RX del adaptador
  - 4 RX del mando → TX del adaptador
  - No conectes 3V3 si el mando ya lleva la pila puesta.
- Monitor serie a **115200 8N1**: el de PlatformIO, *Serial Monitor*.
- Multímetro y, si puede ser, una fuente de laboratorio con límite de corriente.
- Una 18650 cargada y un imán (para el reset por el reed).

## 1. Alimentación (sin micro programado)
1. Sin pila, alimenta por USB-C con la fuente limitada a 300 mA. Mide:
   - VBUS ≈ 5 V;
   - VDD ≈ 4,7 V (VBUS menos el diodo D3);
   - VCC3.3 = 3,3 V.
2. El LED rojo (cargando) puede parpadear sin pila; es normal en el TP4056.
3. Pon la pila **bien orientada**. El + va junto al pad 2 de BT1, el de arriba, cerca del nRF24.
   - Con el USB puesto se enciende el LED rojo y la corriente de carga es de unos 0,5 A.
   - Al acabar se enciende el verde.
4. Quita el USB. VCC3.3 debe seguir a 3,3 V, ahora desde la pila.
5. **Prueba de la pila al revés** (con cuidado): Q4 debe impedir que pase corriente; VCC3.3 = 0 V y nada se calienta.

## 2. Programar
1. En VS Code: **Build** y luego **Upload**, con el ST-Link en PROG.
2. Abre el monitor serie. Debe salir algo así:
   ```
   sailoQ remote v0.1
   EEPROM vacia: valores por defecto
   radio OK, id 1A2B3C4D, bateria 3870 mV
   ```
   - Si pone `radio NO RESPONDE`: revisa que el módulo nRF24 está bien enchufado (por detrás, con la antena hacia el borde de arriba) y las soldaduras de RF1.
3. Suena un pitido corto. Si suena uno largo, la radio no responde.

## 3. Pantalla
1. Debe aparecer "sailoQ remote v0.1" con el estado de radio y batería, y luego la pantalla de AJUSTES (nada está emparejado).
2. **Si no se ve nada:**
   - comprueba la retroiluminación (cable J1: pin 1 = ánodo, pin 2 = cátodo; tarea B5);
   - prueba el contraste: en `main.c` cambia `cfg.contraste` (por defecto 35) entre 20 y 50.
3. **Si se ve al revés, en espejo o desplazada:** cambia `LCD_ADC_REV`, `LCD_SHL_REV` y `LCD_COM0` en `include/st7571.h` (tarea F2) y vuelve a grabar.
4. Ajusta el contraste desde AJUSTES → CONTRASTE con K3 y K4, mirando la pantalla con luz normal y al sol.

## 4. Teclas, zumbador y retroiluminación
1. En AJUSTES:
   - K1/PÁG◄ sube y K2/PÁG► baja;
   - K3 y K4 cambian el valor;
   - K5 sale;
   - K6 es OK.
2. Cada tecla pita si PITIDO = SI.
3. BRILLO de 0 a 100 %: la retroiluminación cambia.

## 5. Bajo consumo
1. Deja el mando sin tocar: a los 50 s baja el brillo y a los 60 s se apaga todo.
2. Mide la corriente de la pila en reposo, con un amperímetro en serie con la pila y el USB quitado. **Objetivo: menos de 20 µA** (tarea F7). Si es mucho más, apunta el valor.
3. Pulsa una tecla: el mando se enciende, pero **esa primera pulsación no hace nada**.
4. Acerca el imán a la marca del reed SW9, en la esquina derecha bajo la pantalla: el mando se reinicia.

## 6. Radio con la base
Esto solo es posible cuando la lite use la librería `sailoQ_radio` (tarea R4 de la librería; protocolo en `../sailoQ_radio/PROTOCOLO_RADIO.md`).
1. AJUSTES → BASE → OK. El mando muestra "BUSCANDO BASE".
2. Pulsa el botón de emparejar de la lite: aparece "EMPAREJADO" y el nombre.
3. Sal con K5: aparecen BIDATA, VIENTO y PILOTO.
4. PILOTO, primero **con el piloto en el puerto y el barco amarrado**:
   - STBY (K5) actúa al momento;
   - AUTO (K6) con pulsación larga;
   - −1/+1/−10/+10 actúan al soltar la tecla.

## 7. Actualizar el firmware sin ST-Link (opcional)
El STM32 trae de fábrica un cargador por puerto serie (USART1, que es la tira DBG).
1. Une el pin 5 (BOOT0) con el pin 2 (3V3) de la tira DBG.
2. Reinicia con el imán.
3. Graba con STM32CubeProgrammer por UART: 115200, paridad *even*.
4. Quita el puente y reinicia.
