//>>> The latest version of this code can be found at https://github.com/jcw/ !!

// Demo sketch for the Thermo Plug v1
// 2009-09-17 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php
// $Id: thermo_demo.pde 7763 2011-12-11 01:28:16Z jcw $

#include <Ports.h>
#include <RF12.h> // needed to avoid a linker error :(

Port tplug (1);

void setup () {
    Serial.begin(57600);
    Serial.println("\n[thermo_demo]");
/*    
    tplug.mode(OUTPUT);
    tplug.digiWrite(1);
    delay(1000);
    tplug.digiWrite(0); */
}

void loop () {
    int t = tplug.anaRead();
//    Serial.print(t);
//    Serial.print(' ');
    Serial.println(map(t, 0, 1024, 0, 3300)); // 10 mv/C
    delay(500);
}
