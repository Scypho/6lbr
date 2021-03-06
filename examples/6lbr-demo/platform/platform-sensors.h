/*
 * Copyright (c) 2015, CETIC.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         6LBR-Demo Project Configuration
 * \author
 *         6LBR Team <6lbr@cetic.be>
 */

#ifndef PLATFORM_SENSORS_H
#define PLATFORM_SENSORS_H

#include "sensors.h"

#ifdef CETIC_6LN_PLATFORM_SENSORS

#include CETIC_6LN_PLATFORM_SENSORS

#else

#if PLATFORM_HAS_SHT11
#include "sht11-sensor.h"

#define SENSOR_INIT_TEMP() SENSORS_ACTIVATE(sht11_sensor)
#define SENSOR_INIT_HUMIDITY() SENSORS_ACTIVATE(sht11_sensor)

#if REST_RES_TEMP_RAW
#define REST_REST_TEMP_VALUE sht11_sensor.value(SHT11_SENSOR_TEMP)
#else
#define REST_REST_TEMP_VALUE (sht11_sensor.value(SHT11_SENSOR_TEMP) - 3960)
#endif

#if REST_RES_HUMIDITY_RAW
#define REST_REST_HUMIDITY_VALUE sht11_sensor.value(SHT11_SENSOR_HUMIDITY)
#else
#define REST_REST_HUMIDITY_VALUE (((uint32_t)sht11_sensor.value(SHT11_SENSOR_HUMIDITY) * 367 - 20468) / 100)
#endif

#endif /* PLATFORM_HAS_SHT11 */

#if PLATFORM_HAS_SHT21

#include "sht21.h"

#define SENSOR_INIT_TEMP()
#define SENSOR_INIT_HUMIDITY()

#if REST_RES_TEMP_RAW
#define REST_REST_TEMP_VALUE sht21_read_temp()
#else
#define REST_REST_TEMP_VALUE (((uint32_t)sht21_read_temp()) * 17572 / 65536 - 4685)
#endif

#if REST_RES_HUMIDITY_RAW
#define REST_REST_HUMIDITY_VALUE sht21_read_humidity()
#else
#define REST_REST_HUMIDITY_VALUE ((((uint32_t)sht21_read_humidity())*12500/65536)-600)
#endif

#endif /* PLATFORM_HAS_SHT21 */

#endif /* CETIC_6LN_PLATFORM_SENSORS */

#endif /* PLATFORM_SENSORS_H */
