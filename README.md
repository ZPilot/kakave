# kakave
<B>- Контроллер псевдо КМД+имитация 4х дисководов для УКНЦ (на SD карте)</B></Br>

<B>Идея:</B></Br>
Сделать простой для повторения и как можно более дешевый контроллер НГМД, который будет работать со встроенным в УКНЦ драйвером.

<B>Реализация:</B></Br>
Контроллер построен на базе платы Blackpill, имеющая на борту stm32f401 или stm32f411.

![alt text](photo/example1.jpeg "Kakave")

<B>Что работает:</B></Br>
Чтение/запись с образов DSK размером 800 кБ, поддерживаются образы имеющие нестандартные размеры, но запись не гарантируется.
Работают: RT-11, Unix и другие ОС, программное обеспечение и игры из-под ОС.

<B>Что не работает:</B></Br>
Нет поддержки каталогов.</Br>
Игры от ИТО: ITO90.dsk - все игры с этого диска, ITO91.dsk - не работает PacMan.

<B>Ошибки:</B></Br>
На герберах перепутаны подписи под кнопками Left и Right. Кнопка Set расположена слишком близко к дисплею и мешает его установке.
В демонстрационном ролике старая модель корпуса с недостоверно подписанными светодиодными индикаторами. Индикаторы отображают сигнал "Мотор" и сигнал чтения/записи.

<B>Обратите внимание:</B></Br>
Платы BlackPill и олед-дисплея имеются в продаже с различающейся разводкой выводов.
При монтаже выводы разъема для программирования SCK и DIO запаиваются на плату.</Br>
<img src="photo/blackpill.jpg" alt="blackpill" width="300"/>
<img src="photo/oled.jpeg" alt="blackpill" width="200"/>

<B>Пример работы контроллера:</B></Br>
[![Example video](https://img.youtube.com/vi/BQEt_0jdZwQ/0.jpg)](https://youtu.be/BQEt_0jdZwQ "Example video")
</Br>
<B>Сообществом были произведены следующие улучшения:</B></Br>
Новая трассировка платы и гербер-файлы: <a href="Community/Gerber_new/">Community/Gerber_new/</a></Br>
<img src="Community/photo/photo_5327789880400529236_y.jpg" alt="внешний вид платы" width="300"/></Br>
Стильный корпус со сменными передними накладками, там же несколько моделей в сборе на "покрутить":<a href="Community/Case/">Community/Case/</a></Br>
<img src="Community/photo/photo_5327789880400529243_x.jpg" alt="1" width="300"/></Br>
<img src="Community/photo/photo_5327789880400529244_x.jpg" alt="1" width="300"/></Br>
<img src="Community/photo/photo_5327789880400529245_x.jpg" alt="1" width="300"/></Br>
<img src="Community/photo/photo_5327789880400529246_x.jpg" alt="1" width="300"/></Br>
<img src="Community/photo/photo_5327789880400529247_x.jpg" alt="1" width="300"/></Br>
<B>Авторы модов:</B></Br>
@electroscatnes</Br>
Andrey Khristov</Br>
<B>Идейные вдохновители:<B></Br>
Alexey Kisly</Br>
@nzeemin</Br>
и все участники канала: <a href="https://t.me/MC0511UKNC">https://t.me/MC0511UKNC</a>


