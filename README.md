# Sensor de suelo RS485 con Heltec WiFi LoRa 32 V3 (ESP-IDF)

Firmware en C para leer un sensor de suelo de 7 parametros por RS485/Modbus RTU
con una **Heltec WiFi LoRa 32 V3**. La placa usa un ESP32-S3 y el proyecto usa
exclusivamente APIs de ESP-IDF (`driver/uart`, `driver/gpio`, FreeRTOS y
`esp_err`); no contiene Arduino ni archivos `.ino`.

El programa consulta pH, humedad, temperatura, conductividad electrica (EC),
nitrogeno, fosforo y potasio, valida el CRC Modbus y escribe una linea JSON por
el monitor serial cada cinco segundos.

> Estado de integracion: el firmware implementa el mapa del manual
> **Soil Multi-parameter Sensor Manual V2.2** (sensor de 7 parametros, direccion
> `0x02`, 9600 8N1): una sola consulta de 8 registros desde `0x0000`. Si su unidad
> trae otro manual, compare los registros de la seccion [Perfil Modbus](#perfil-modbus).
> No se debe asumir que todos los sensores "7 en 1" comparten los mismos registros.

## Plataforma y version

| Elemento | Valor |
| --- | --- |
| Placa | Heltec WiFi LoRa 32 V3 |
| MCU/target | ESP32-S3 / `esp32s3` |
| Framework previsto | ESP-IDF v5.3 o posterior |
| Interfaz del sensor | RS485, Modbus RTU, 8-N-1, 9600 bit/s por defecto |
| Direccion Modbus por defecto | `0x02` (segun el manual V2.2 del sensor) |

La V3 integra el ESP32-S3 y un SX1262; el radio LoRa no se configura en este
proyecto porque no se especificaron frecuencia, region ni receptor. El uso de
la placa aqui es como maestro Modbus. La asignacion de pines elegida evita los
pines ocupados por la radio y la pantalla.

## Materiales

- Heltec WiFi LoRa 32 V3 y cable USB-C.
- Sensor de suelo RS485 de 7 parametros (TH, EC, pH, N, P y K).
- Modulo **HW-726** (convertidor UART TTL a RS485 con control automatico de
  direccion). Alimentelo a 3.3 V desde la Heltec.
- Fuente regulada apropiada para el sensor y cable RS485. Para tendidos largos,
  use par trenzado y termine el bus con 120 ohmios en los extremos.

## Conexion

El sensor no se conecta a los GPIO directamente: RS485 es diferencial y
necesita un transceptor. La siguiente tabla usa los cuatro colores que aparecen
en la ficha recibida (verifique siempre la etiqueta/cableado de su unidad).

| Sensor | HW-726 | Heltec V3 |
| --- | --- | --- |
| Rojo, P+ | fuente positiva del sensor | `Vext` de 3.3 V **solo si** el sensor fue verificado a 3.3 V; si no, fuente externa de 5-24 V |
| Negro, P- | GND de fuente y GND del transceptor | GND comun |
| Amarillo, 485A+ | A+ | - |
| Verde, 485B- | B- | - |
| - | RXD | GPIO4 (TX de UART1) |
| - | TXD | GPIO5 (RX de UART1) |
| - | VCC / GND | 3V3 / GND |

El HW-726 conmuta automaticamente entre enviar y recibir, por lo que **GPIO6
no se conecta ni se usa en el firmware**. En la Heltec V3, GPIO4 y GPIO5 son
los pines predeterminados del proyecto. El cruce TX/RX es intencional: el TX de
la placa llega al RXD del modulo y el TXD del modulo llega al RX de la placa.
Si el sensor usa alimentacion externa, su P- debe compartir tierra con el
HW-726 y la Heltec. Nunca aplique 5 V o 12 V a un GPIO de la placa.

`Vext` es salida de 3.3 V con control activo en bajo por GPIO36. Se deja
desactivada por defecto en el firmware: habilitela solo despues de confirmar que
su sensor funciona a 3.3 V y que su consumo cabe en el limite de Vext. Con una
fuente externa no habilite esa opcion.

## Estructura

```text
.
|- main/                         aplicacion: UART para HW-726 y bucle de lectura
|- components/soil_sensor/       protocolo Modbus puro y conversion de escalas
|  |- include/soil_sensor.h      interfaz publica del componente
|  `- test/                      pruebas Unity del componente
|- test/                         aplicacion ESP-IDF dedicada a pruebas
|- CMakeLists.txt                proyecto ESP-IDF
|- sdkconfig.defaults            configuracion base para ESP32-S3
|- sdkconfig                     semilla de configuracion del target
|- LICENSE                       licencia MIT
`- .gitignore
```

## Compilar y flashear

1. Instale y active ESP-IDF v5.3 o posterior siguiendo la guia oficial. En Windows, abra
   **ESP-IDF PowerShell** para que `idf.py` y `IDF_PATH` esten disponibles.
2. Confirme el entorno:

   ```powershell
   idf.py --version
   ```

3. Desde la raiz del repositorio, seleccione el target y compile:

   ```powershell
   idf.py set-target esp32s3
   idf.py build
   ```

   Si se habia intentado compilar una version anterior del proyecto, ejecute
   una vez `idf.py fullclean` antes de `idf.py build` para regenerar CMake.

4. Con la placa conectada, reemplace `COMx` por su puerto y flashee:

   ```powershell
   idf.py -p COMx flash monitor
   ```

   Salga del monitor con `Ctrl-]`. Una lectura valida luce asi:

   ```json
   {"temperature_c":24.1,"moisture_percent":27.8,"ec_us_cm":1234,"ph":6.54,"nitrogen_mg_kg":32,"phosphorus_mg_kg":37,"potassium_mg_kg":48}
   ```

Los valores seriales (pines, baudios, direccion, periodo y Vext) se pueden
cambiar sin editar C mediante:

```powershell
idf.py menuconfig
```

Ruta: `Soil RS485 sensor`.

## Perfil Modbus

El componente genera una solicitud Modbus 0x03, espera la respuesta y comprueba
longitud, direccion, funcion, contador de bytes y CRC-16 antes de convertir datos.
Se lee todo en una sola consulta (`00 03 00 00 00 08` para broadcast; con la
direccion 2 es `02 03 00 00 00 08 44 3F`). Mapa del manual V2.2:

| Registro | Medida | Escala aplicada |
| --- | --- | --- |
| `0x0000` | Temperatura | entero con signo / 10 C |
| `0x0001` | Humedad | valor / 10 % |
| `0x0002` | EC | uS/cm directo |
| `0x0003` | Salinidad (salt) | directo |
| `0x0004` | Nitrogeno | mg/kg directo |
| `0x0005` | Fosforo | mg/kg directo |
| `0x0006` | Potasio | mg/kg directo |
| `0x0007` | pH | valor / 10 |

Notas del fabricante: el pH usa electrodos zinc-aluminio y la primera medicion
tarda mas de 5 minutos en estabilizarse (rango 3 a 9 pH). La direccion se cambia en
el registro `0x0080` y los baudios en `0x0081`.

## Pruebas unitarias (Unity)

La prueba se compila en una aplicacion separada, de modo que no se incluye en
el firmware de produccion. Cubre la trama de consulta del manual, el rechazo de una
respuesta con CRC alterado y las conversiones, incluida temperatura negativa.

```powershell
cd test
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

El resultado esperado termina con `0 Failures` y `OK`. La prueba no necesita
tener el sensor conectado porque prueba la capa de protocolo/decodificacion.

## Diagnostico rapido

| Sintoma | Revision |
| --- | --- |
| `timeout` | Compruebe alimentacion, P-/GND comun, A/B, direccion Modbus y que el baud coincida (muchas unidades llegan a 9600 o 4800). |
| No hay respuesta tras cablear todo | Invierta A y B una vez: algunos fabricantes rotulan la polaridad al reves. |
| Respuesta `invalid Modbus` | Revise ruido, terminacion, direccion y parametros seriales. El firmware descarta tramas con CRC incorrecto. |
| pH, humedad o temperatura implausibles | Confirme el mapa del manual del sensor; no cambie escalas solo por intuicion. |
| Reinicios o nivel alto en RX | Cambie a un transceptor de 3.3 V. Un modulo MAX485 alimentado a 5 V puede enviar 5 V por RO. |


## Referencias

- [Heltec WiFi LoRa 32: recursos de V3, esquema y pin map](https://docs.heltec.org/en/node/esp32/wifi_lora_32/related_links.html)
- [Heltec WiFi LoRa 32: ESP32-S3, SX1262 y comparacion de versiones](https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html)
- [Guia ESP-IDF: sistema de compilacion con CMake](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)
- [Guia ESP-IDF: pruebas unitarias con Unity](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/unit-tests.html)
- Soil Multi-parameter Sensor Manual V2.2 (manual del fabricante entregado con el sensor)
