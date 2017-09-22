# OALSFXPP

A standalone OpenAL Soft effects for C++.


Contents
========
1. Disclaimer
2. Overview
3. Build requirements


1 - Disclaimer
==============

Copyright (C) 1999-2017 OpenAL Soft authors.  
Copyright (C) 2017 Boris I. Bendovsky (<bibendovsky@hotmail.com>)

This program is free software; you can redistribute it and/or  
modify it under the terms of the GNU General Public License  
as published by the Free Software Foundation; either version 2  
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of  
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
GNU General Public License for more details.

You should have received a copy of the GNU General Public License  
along with this program; if not, write to the Free Software  
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

For a copy of the GNU General Public License see file COPYING.  


2 - Overview
============

A standalone OpenAL Soft effects for C++.
Based on OpenAL Soft v1.18.1 (http://kcat.strangesoft.net/openal.html).

Available effects:
  * Chorus
  * Compressor
  * Dedicated (dialog)
  * Dedicated (low frequency)
  * Distortion
  * Echo
  * Equalizer
  * Flanger
  * Ring modulator
  * Reverb
  * Reverb (EAX)

Up to four effects can be used simultaneously.


3 - Build requirements
======================

Minimum requirements:
  * C++14 compatible compiler.
  * CMake 3.5.1 (for test program only).
