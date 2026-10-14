# Sensor de suelo RS485 con Heltec WiFi LoRa 32 V3 (ESP-IDF)

Firmware en C para leer un sensor de suelo de 7 parámetros por RS485/Modbus RTU
con una **Heltec WiFi LoRa 32 V3**. La placa usa un ESP32-S3 y el proyecto usa
exclusivamente APIs de ESP-IDF (`driver/uart`, `driver/gpio`, FreeRTOS, `esp_err`,
`esp_log`); no contiene Arduino ni archivos `.ino`.

El programa consulta temperatura, humedad, conductividad eléctrica (EC), salinidad,
nitrógeno, fósforo, potasio y pH, valida el CRC Modbus y escribe una línea JSON por
el monitor serial cada cinco segundos (configurable).

> Estado de integración: el firmware implementa el mapa del manual **Soil
> Multi-parameter Sensor Manual V2.2** (sensor de 7 parámetros, dirección `0x02`,
> 9600 8N1): una sola consulta de 8 registros desde `0x0000`. Si su unidad trae otro
> manual, compare los registros de la sección [Perfil Modbus](#perfil-modbus).
> No se debe asumir que todos los sensores "7 en 1" comparten los mismos registros.

## Plataforma y versión

| Elemento                     | Valor                                            |
| ---------------------------- | ------------------------------------------------ |
| Placa                        | Heltec WiFi LoRa 32 V3                           |
| MCU / target                 | ESP32-S3 / `esp32s3`                             |
| Versión de ESP-IDF           | **v6.1** (la usada para compilar y probar)       |
| Interfaz del sensor          | RS485, Modbus RTU, 8-N-1, 9600 bit/s por defecto |
| Dirección Modbus por defecto | `0x02` (según el manual V2.2 del sensor)         |

La V3 integra el ESP32-S3 y un SX1262; el radio LoRa no se configura en este
proyecto porque no se especificaron frecuencia, región ni receptor. La placa se usa
aquí como maestro Modbus. La asignación de pines elegida evita los pines ocupados
por la radio y la pantalla.

## Materiales

- Heltec WiFi LoRa 32 V3 y cable USB-C.
- Sensor de suelo RS485 de 7 parámetros (TH, EC, pH, N, P y K).
- Módulo **HW-726** (convertidor UART TTL a RS485 con control automático de
  dirección). Aliméntelo a 3.3 V desde la Heltec.
- Fuente regulada apropiada para el sensor y cable RS485. Para tendidos largos,
  use par trenzado y termine el bus con 120 ohmios en los extremos.

## Conexión

El sensor no se conecta a los GPIO directamente: RS485 es diferencial y necesita un
transceptor. La siguiente tabla usa los cuatro colores de la ficha recibida
(verifique siempre la etiqueta/cableado de su unidad).

| Sensor          | HW-726                              | Heltec V3                                                                                     |
| --------------- | ----------------------------------- | --------------------------------------------------------------------------------------------- |
| Rojo, P+        | fuente positiva del sensor          | `Vext` de 3.3 V **solo si** el sensor fue verificado a 3.3 V; si no, fuente externa de 5-24 V |
| Negro, P-       | GND de fuente y GND del transceptor | GND común                                                                                     |
| Amarillo, 485A+ | A+                                  | -                                                                                             |
| Verde, 485B-    | B-                                  | -                                                                                             |
| -               | RXD                                 | GPIO4 (TX de UART1)                                                                           |
| -               | TXD                                 | GPIO5 (RX de UART1)                                                                           |
| -               | VCC / GND                           | 3V3 / GND                                                                                     |

El HW-726 conmuta automáticamente entre enviar y recibir, por lo que **GPIO6 no se
conecta ni se usa en el firmware**. En la Heltec V3, GPIO4 y GPIO5 son los pines
predeterminados del proyecto. El cruce TX/RX es intencional: el TX de la placa llega
al RXD del módulo y el TXD del módulo llega al RX de la placa. Si el sensor usa
alimentación externa, su P- debe compartir tierra con el HW-726 y la Heltec. Nunca
aplique 5 V o 12 V a un GPIO de la placa.

`Vext` es una salida de 3.3 V con control activo en bajo por GPIO36. Se deja
desactivada por defecto en el firmware: habilítela solo después de confirmar que su
sensor funciona a 3.3 V y que su consumo cabe en el límite de Vext. Con una fuente
externa no habilite esa opción.

## Estructura del proyecto

```
.
|- main/                         aplicación: UART para HW-726 y bucle de lectura
|  |- main.c
|  |- Kconfig.projbuild          opciones de menuconfig (pines, baudios, dirección...)
|  `- CMakeLists.txt
|- components/soil_sensor/       protocolo Modbus puro y conversión de escalas
|  |- include/soil_sensor.h      interfaz pública del componente
|  |- soil_sensor.c
|  `- test/                      pruebas Unity del componente
|- test/                         aplicación ESP-IDF dedicada a las pruebas
|  `- main/test_app_main.c       lanza todas las pruebas Unity
|- CMakeLists.txt                proyecto ESP-IDF
|- sdkconfig.defaults            configuración base para ESP32-S3
|- sdkconfig                     configuración del target
|- LICENSE                       licencia MIT
`- .gitignore
```

## Compilar y flashear

1. Instale y active **ESP-IDF v6.1** siguiendo la guía oficial. En Windows, abra
   **ESP-IDF PowerShell** para que `idf.py` y `IDF_PATH` estén disponibles.

2. Confirme el entorno:

   ```
   idf.py --version
   ```

3. Desde la raíz del repositorio, seleccione el target y compile:

   ```
   idf.py set-target esp32s3
   idf.py build
   ```

   Si antes se compiló otra versión del proyecto, ejecute una vez
   `idf.py fullclean` antes de `idf.py build` para regenerar CMake.

4. Con la placa conectada, reemplace `COMx` por su puerto y flashee:

   ```
   idf.py -p COMx flash monitor
   ```

   Salga del monitor con `Ctrl-]`. Una lectura válida luce así (valores de ejemplo):

   ```
   {"temperature_c":24.1,"moisture_percent":27.8,"ec_us_cm":1234,"salt":617,"ph":6.5,"nitrogen_mg_kg":32,"phosphorus_mg_kg":37,"potassium_mg_kg":48}
   ```

Los valores seriales (pines, baudios, dirección, período y Vext) se pueden cambiar
sin editar C mediante:

```
idf.py menuconfig
```

Ruta: `Soil RS485 sensor`.

## Perfil Modbus

El componente genera una solicitud Modbus 0x03, espera la respuesta y comprueba
longitud, dirección, función, contador de bytes y CRC-16 antes de convertir datos.
Se lee todo en una sola consulta (`00 03 00 00 00 08` para broadcast; con la
dirección 2 es `02 03 00 00 00 08 44 3F`). Mapa del manual V2.2:

| Registro | Medida           | Escala aplicada         |
| -------- | ---------------- | ----------------------- |
| `0x0000` | Temperatura      | entero con signo / 10 C |
| `0x0001` | Humedad          | valor / 10 %            |
| `0x0002` | EC               | uS/cm directo           |
| `0x0003` | Salinidad (salt) | directo                 |
| `0x0004` | Nitrógeno        | mg/kg directo           |
| `0x0005` | Fósforo          | mg/kg directo           |
| `0x0006` | Potasio          | mg/kg directo           |
| `0x0007` | pH               | valor / 10              |

Notas del fabricante: el pH usa electrodos zinc-aluminio y la primera medición tarda
más de 5 minutos en estabilizarse (rango 3 a 9 pH). La dirección se cambia en el
registro `0x0080` y los baudios en `0x0081`.

## Pruebas unitarias (Unity)

Las pruebas usan el framework **Unity** de ESP-IDF y viven en
`components/soil_sensor/test/`. Se compilan en una aplicación separada (`test/`), de
modo que no se incluyen en el firmware de producción. Cubren:

- la trama de consulta del manual,
- el rechazo de una respuesta con CRC alterado,
- la decodificación de un registro big endian,
- la conversión de los 8 registros, incluida temperatura negativa.

Para ejecutarlas (con la placa conectada por USB; no necesita el sensor, porque
prueba la capa de protocolo/decodificación):

```
cd test
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

Resultado obtenido con ESP-IDF v6.1 en la Heltec WiFi LoRa 32 V3:

```
Running Modbus request matches the manual example (8 registers from 0x0000)...
test_soil_sensor.c:5:Modbus request matches the manual example (8 registers from 0x0000):PASS
Running Modbus response is rejected if the CRC changes...
test_soil_sensor.c:15:Modbus response is rejected if the CRC changes:PASS
Running Modbus response decodes a big endian holding register...
test_soil_sensor.c:24:Modbus response decodes a big endian holding register:PASS
Running Manual example response decodes all 8 registers...
test_soil_sensor.c:34:Manual example response decodes all 8 registers:PASS

-----------------------
4 Tests 0 Failures 0 Ignored
OK
```

## Diagnóstico rápido

| Síntoma                                | Revisión                                                                                                                   |
| -------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| `timeout`                              | Compruebe alimentación, P-/GND común, A/B, dirección Modbus y que el baud coincida (muchas unidades usan 9600 o 4800).     |
| No hay respuesta tras cablear todo     | Invierta A y B una vez: algunos fabricantes rotulan la polaridad al revés.                                                 |
| Respuesta `invalid Modbus`             | Revise ruido, terminación, dirección y parámetros seriales. El firmware descarta tramas con CRC incorrecto.                |
| pH, humedad o temperatura implausibles | Confirme el mapa del manual del sensor; no cambie escalas solo por intuición.                                              |
| Reinicios o nivel alto en RX           | Cambie a un transceptor de 3.3 V. Un módulo MAX485 alimentado a 5 V puede enviar 5 V por RO.                               |

## Licencia

Este proyecto se distribuye bajo la licencia **MIT**. Consulte el archivo
[LICENSE](LICENSE).

## Referencias

- [Heltec WiFi LoRa 32: recursos de V3, esquema y pin map](https://docs.heltec.org/en/node/esp32/wifi_lora_32/related_links.html)
- [Heltec WiFi LoRa 32: ESP32-S3, SX1262 y comparación de versiones](https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html)
- [Guía ESP-IDF: sistema de compilación con CMake](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)
- [Guía ESP-IDF: pruebas unitarias con Unity](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/unit-tests.html)
- Soil Multi-parameter Sensor Manual V2.2 (manual del fabricante entregado con el sensor)[S28af37c0df36495d9361740ec0d9bdfbk](S28af37c0df36495d9361740ec0d9bdfbk).
