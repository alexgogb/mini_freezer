# Mininevera con Peltier

Autor: Alejandro González Blanco (alexgogb)

## Introducción

El efecto Peltier es un fenómeno físico termoeléctrico por el cual a un dispositivo termoeléctrico se le aplica un voltaje crea a su vez una diferencia de temperatura. Para aprovechar esta propiedad, existen componentes denominados células o celdas de Peltier, compuestos por dos caras externas de un material diélectrico cerámico y por metales semiconductores tipo P y N que forman los denominados termopares. El movimiento de electrones en el dispositivo surgido de aplicar un voltaje a los termopares provoca que por una cara se enfríe y por la otra se caliente, por lo que si refrigeramos la parte caliente, dicho calor no se transmitirá a la parte fría. Así se consigue un potente refrigerador silencioso y sin partes móviles, aunque con un elevado consumo de más de 4 amperios en los modelos más básicos.

A partir de este componente nace la idea de crear un sistema empotrado en formato de mininevera que permita enfríar botellas o bebidas de forma cómoda y en un formato compacto. A lo largo de este documento se detallarán los objetivos establecidos, el software y el hardware desarrollados, así como la lista de materiales (BOM) y la carcasa diseñada.

## Objetivos

- Diseñar un sistema similar a un electrodoméstico.
- Diseñar un sistema completamente funcional y práctico.
- Diseñar un sistema utilizando componentes reciclados de mi laboratorio personal y de dispositivos estropeados.

¿Por qué un electrodoméstico? Porque es el sistema empotrado por excelencia. No hay mejor muestra de un sistema empotrado que un electrodoméstico o un automóvil debido a que su electrónica y su firmware están creados con numerosas restricciones en mente, ya sea de potencia de cómputo, de memoria o restricciones de espacio.

¿Por qué funcional y práctico? Porque el proceso de creación de un producto utilizable por los usuarios es muy valioso para el aprendizaje en un sector como este.

¿Por qué componentes reciclados? Porque abarata los costes y permite darles una segunda vida a materiales y componentes electrónicos funcionales que, en otra situación, estarían en un vertedero siendo un desperdicio de recursos.

## Arquitectura

![Arquitectura propuesta](/images/architecture_diagram.png)

Como se puede observar en el diagrama, se propone un sistema con entradas y salidas claras y modulares, pero que interactúan entre sí cuando es necesario.

## Hardware

El microcontrolador elegido para el desarrollo de este sistema es una ESP32 C6 de Espressif Systems. Dicho microcontrolador se diferencia del resto de ESP32 de Espressif en que no es de arquitectura Xtensa, sino RISC V: una arquitectura open source que está en auge en los últimos años.

![ESP32 C6](/images/esp32_c6.png)

Además, el núcleo principal del diseño es una célula de Peltier, descrito anteriormente en la introducción. Esta célula de Peltier tiene una corriente de trabajo de 4 amperios y funciona a 12 voltios.

![Célula de Peltier](/images/peltier.jpg)

La célula TEC 12706, que es de las más baratas disponibles en internet, consigue llegar con un disipador reciclado de un ordenador a unos 1,4ºC, como se puede observar en la imagen.

![Célula de Peltier a 1,4ºC](/images/minimum_temperature_peltier.jpg)

Por otro lado, el sistema incorpora los siguientes componentes para imitar el funcionamiento de una nevera:

- Un ventilador interno para distribuir el frío en el compartimento de refrigeración.
- Un disipador activo para disipar el calor generado por la Peltier.
- Luces LED.
- Un reed switch para detectar la apertura de puerta.
- Botones.
- Un buzzer que pita cuando se pulsa un botón o se abre la puerta durante mucho tiempo.
- Un sensor de temperatura y humedad. para saber a cuánta temperatura está el compartimento de refrigeración.
- MOSFETs de nivel lógico para controlar la conmutación de la Peltier y controlar los motores.
- Una pantalla LCD para que el usuario tenga una interfaz con la que interactuar.
- Una fuente de alimentación de 12V.
- Un convertidor buck para reducir de 12V a 5V.

El sistema se divide en el módulo de potencia y en el módulo lógico. Para el módulo de potencia se utilizan MOSFETs de potencia de nivel lógico IRLZ44N debido a que deben manejar grandes corrientes de más de 4 amperios y así alimentar al sistema. Para el módulo lógico se usan los componentes que dan funcionalidad al diseño, listados anteriormente.

A continuación, una imagen del circuito interno con todos los componentes soldados a placas perforadas y el sistema completamente empotrado en la carcasa.

![Circuito interno](/images/internal_circuit.jpg)

## Software

Un electrodoméstico real utilizaría controladores a los que el microcontrolador manda comandos para poder actuar en concurrencia. Sin embargo, por limitaciones de presupuesto no es viable adquirir un controlador por cada módulo. Para solucionar dicho problema, se propone la creación de 5 hilos en concurrencia encargados cada uno de controlar aspectos que necesitan estar ejecutándose a la vez y no pueden ser secuenciales. Los hilos en FreeRTOS se conocen como tasks o tareas, nombre utilizado de aquí en adelante.

| Tarea | Función |
|-------|---------|
| Principal | Bucle principal y enfriamiento |
| Botones | Getsión de la entrada de usuario con los botones |
| Temperatura | Leer el sensor de temperatura y humedad (detallado posteriormente) |
| Apertura de puerta | Detección de la apertura de puerta y control de los timers y las interrupciones relacionadas |
| Audio | Control del audio del sistema |

El control de las tareas se realiza mediante semáforos, mutex y colas de eventos. Las colas de eventos FIFO son las principales estructuras de datos utilizadas debido a que permiten simular el envío de comandos a controladores como envío de eventos a tareas.

Durante la primera parte del proyecto, y siguiendo el principio de reciclaje, fue implementada una librería de control de la pantalla LCD mediante un registro de desplazamiento 74HCT595 de Texas Instruments para ahorrar pines, utilizando únicamente tres. Esto fue así debido a la falta de una pantalla con I2C que permitiera utilizar tan solo dos pines. Para su desarrollo, fue necesaria la lectura de la hoja de datos y el uso de bit-banging para simular el protocolo que entiende la LCD1602. Tras un tiempo con dicha librería, se pudo tener acceso a una pantalla con un adaptador I2C y se sustituyó el anterior circuito.

### Flujo de trabajo

El sistema inicia dando la bienvenida al usuario. A continuación, solicita al usuario el modo de enfriamiento, que configura el PWM de control de la Peltier.

![Selección de modo](/images/mode_selection.jpg)

Posteriormente, solicita el tiempo que estará enfriando la nevera, como si fuera un microondas.

![Selección de tiempo](/images/time_selection.jpg)

Finalmente, se muestran la temperatura, la humedad y el tiempo restante.

![Enfriando](/images/working.jpg)

Si la puerta se abre, se congela el contador y se muestra un aviso. Si pasan unos segundos, sonará una alarma. Si pasan más segundos, se apagará el sistema de enfriamiento.

![Puerta abierta](/images/door_open.jpg)
![Puerta abierta 2](/images/door_open_2.jpg)

## Carcasa

Para la construcción de la carcasa, se ha optado por madera MDF, Medium-density fiberboard, porque la madera es un material asilante, resistente y robusto, características ideales para una nevera que debe mantener el frío interno y soportar el peso de los componentes. Asimismo, se ha forrado el interior del compartimento de refrigeración con poliestireno y en la puerta se ha colocado en el perímetro un burlete de goma para hacer presión contra el marco y aislar mejor. La parte trasera cuenta con agujeros para permitir el flujo del aire del disipador y la puerta tiene dos imanes para su cierre magnético y un tercer imán para la detección de apertura de puerta mediante el reed ubicado en el circuito.

![Carcasa](/images/freezer.jpg)
![Parte trasera](/images/back.jpg)
![Interior](/images/interior.jpg)
![Parte superior](/images/top.jpg)
![Parte superior abierta](/images/top_open.jpg)
![Puerta](/images/door.jpg)

## Problemas y soluciones

### Estabilidad de la señal

El uso de un disipador y un motor para un ventilador auxiliar introducía mucho ruido en el circuito, impidiendo el correcto funcionamiento del sistema. Para solventarlo, se optó por poner condensadores de desacoplo donde fuera necesario y soldar todos los componentes.

### Watchdog

El uso de la librería dht.h de esp-lib para la comunicación con el sensor de temperatura tiene un inconveniente en la ESP32 C6, de arquitectura RISC V. En el resto de placas Xtensa, la librería funciona correctamente, pero en la C6 no. Dicho fallo se debe a que la librería usa un mutex que bloquea la CPU, pero en dicho mutex hace una espera mediante vTaskDelay de muchos milisegundos. Todas las secciones críticas deben ser mínimas para evitar bloqueos. Sin embargo, esto no es así en dht.h. Aunque en las ESP32 Xtensa esto no supone un problema al tener dos núcleos, la C6 dispone de únicamente uno, por lo que un bloqueo de tanto tiempo dispara el watchdog o perro guardián, un circuito que reinicia el sistema para llevarlo a un estado seguro ante bloqueos largos de la CPU.

Para solventarlo, se optó por poner en una tarea aparte la lectura del DHT, respetando unos tiempos elevados de espera y dándole una prioridad alta para que el protocolo funcione correctamente.

### Carpintería

La carcasa está hecha principalmente de madera, requiriendo mucho esfuerzo para que quedara bien. Hubo múltiples problemas durante la construcción, como tablas con cortes que no son exactamente rectos, tamaños que no coinciden o ubicación de los componentes con la restricción del disipador y de la fuente de alimentación, los elementos más voluminosos y pesados que condicionaban el resto del diseño.

La solución fue dedicarle horas y planificar minuciosamente dónde estarían dispuestos los circuitos, así como las juntas entre las tablas y sus conexiones.

### Disipación de la Peltier

La célula se calienta rápidamente si no está unida a un disipador por su parte caliente. Por ello, el disipador debe tener una buena conexión con la célula. No obstante, por la forma vertical del disipador de CPU utilizado, la unión entre ambos componentes es muy sensible, por lo que los movimientos bruscos podrían descolocar el disipador, impidiendo el enfriamiento.

La solución más clara a esto es mejorar el diseño de la carcasa y utilizar disipadores horizontales, más compactos y apropiados para un diseño como este. Asimismo, esta mejora permitiría reducir el tamaño del compartimento para el circuito y ceder espacio al compartimento de refrigeración, que es la parte que interesa.

## Materiales (BOM)

A continuación, se describe la tabla de componentes. En coste se detalla el precio para mí y en precio real lo que costaría de verdad. Si son 0€ es porque o bien ya disponía de ellos o bien formaban parte del kit de clase. La mayor parte de componentes son de tiendas como AliExpress, por lo que su coste no es alto.

| Componente | Coste | Precio real |
|------------|-------|-------------|
| Esp32 C6 oficial | 0€ | 8€ |
| Peltier | 8€ | 8€ |
| Disipador | 0€ | 15€ |
| Ventilador + motor | 0€ | 3€ |
| Fuente de alimentación | 0€ | 18€ |
| MOSFET IRLZ44N x3 | 0€ | 4€ |
| LED x2 | 0€ | 0,2€ |
| Buzzer | 0€ | 1€ |
| Pantalla LCD + I2C | 5€ | 5€ |
| Reed switch | 1€ | 1€ |
| Bisagras x4 | 4€ | 4€ |
| Tornillos | 0€ | 1€ |
| Imprimación | 10€ | 10€ |
| Madera MDF | 12€ | 12€ |
| Botones | 0€ | 0,3€ |
| Cables | 0€ | 5€ |
| Resistencias | 0€ | 1€ |
| Diodo | 0€ | 0,2€ |
| Sockets y headers | 0€ | 2€ |
| Condensadores | 0€ | 1€ |
| Poliestireno | 10€ | 10€ |
| Burletes de goma | 6€ | 6€ |
| DHT11 | 0€ | 2€ |
| **Total** | **56€** | **117,7** |

Como se puede observar, la ventaja de utilizar componentes reciclados es notoria económicamente, al menos a nivel electrónico, debido a que reutilizar aislantes como burletes o poliestireno es más complicado.

## Enlace al repositorio de GitHub

[Repositorio de GitHub](https://github.com/alexgogb/mini_freezer)