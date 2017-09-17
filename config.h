/* 
 * The MIT License (MIT)
 * 
 * Copyright (c) 2016 Johan Kanflo (github.com/kanflo)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


//#define CONFIG_NO_WIFI // Skip wifi
//#define CONFIG_NO_PIR  // Skip PIR task

#define CONFIG_TARGET_ESPARDUCAM
//#define CONFIG_TARGET_ESPARDUCAM_MINI

#define ARDUCAM_nPWR  (15) // Camera power enable

#ifdef CONFIG_TARGET_ESPARDUCAM_MINI
 #define LED_nPWR      (16) // LED enable (0 lights the LED)
 #define USR_BUTTON     (5) // User button
#endif // CONFIG_TARGET_ESPARDUCAM_MINI