/* This file is part of Aethered, a collection of virtual device drivers for
 * Windows.
 *
 * Copyright (C) 2010-2011 Alexander Fokin <apfokin@gmail.com>
 *
 * Aethered is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Aethered is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with Aethered. If not, see <http://www.gnu.org/licenses/>. */
#include <stdio.h>
#include <aemctl.h>
#pragma comment(lib, "aemctl.lib")

int main() {
  int a, b, n = 0;
  AemGetDeviceInfo(&a, &b);
  printf("%d %d\n\n", a, b);
  
  while(1) {
    int x, y, buttons, interval, i;
    AEMCTLRESULT result;
    if(n == 0)
      scanf("%d%d%d", &x, &y, &buttons);
    else {
      x = y = 1;
      buttons = 0;
      n--;
    }

    if(x == -1) {
      result = AemClearMessageQueue();
    } else if(x == -2) {
      result = AemGetMessageCheckInterval(&interval);
      printf("interval %d\n", interval);
    } else if(x == -3) {
      result = AemSetMessageCheckInterval(y);
    } else if(x == -4) {
      result = AemGetMessageQueueSize(&b);
      printf("queue size %d\n", b);
    } else if(x == -5) {
      n = y;
      continue;
    } else
      result = AemSendMessage(x, y, buttons);


    if(result != AEMCTL_OK)
      printf("%s\n", AemGetLastErrorString());
    else
      printf("OK!\n");
  }
}

