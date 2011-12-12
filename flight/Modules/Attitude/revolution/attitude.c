/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup Attitude Copter Control Attitude Estimation
 * @brief Acquires sensor data and computes attitude estimate
 * Specifically updates the the @ref AttitudeActual "AttitudeActual" and @ref AttitudeRaw "AttitudeRaw" settings objects
 * @{
 *
 * @file       attitude.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Module to handle all comms to the AHRS on a periodic basis.
 *
 * @see        The GNU Public License (GPL) Version 3
 *
 ******************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/**
 * Input objects: None, takes sensor data via pios
 * Output objects: @ref AttitudeRaw @ref AttitudeActual
 *
 * This module computes an attitude estimate from the sensor data
 *
 * The module executes in its own thread.
 *
 * UAVObjects are automatically generated by the UAVObjectGenerator from
 * the object definition XML file.
 *
 * Modules have no API, all communication to other modules is done through UAVObjects.
 * However modules may use the API exposed by shared libraries.
 * See the OpenPilot wiki for more details.
 * http://www.openpilot.org/OpenPilot_Application_Architecture
 *
 */

#include "pios.h"
#include "attitude.h"
#include "magnetometer.h"
#include "accels.h"
#include "gyros.h"
#include "attitudeactual.h"
#include "attitudesettings.h"
#include "flightstatus.h"
#include "CoordinateConversions.h"

// Private constants
#define STACK_SIZE_BYTES 1540
#define TASK_PRIORITY (tskIDLE_PRIORITY+3)

#define F_PI 3.14159265358979323846f
#define PI_MOD(x) (fmod(x + F_PI, F_PI * 2) - F_PI)
// Private types

// Private variables
static xTaskHandle sensorTaskHandle;
static xTaskHandle attitudeTaskHandle;

static xQueueHandle gyroQueue;
static xQueueHandle accelQueue;
static xQueueHandle magQueue;
const uint32_t SENSOR_QUEUE_SIZE = 10;

// Private functions
static void SensorTask(void *parameters);
static void AttitudeTask(void *parameters);

static int32_t updateSensors();
static int32_t updateAttitudeComplimentary();
static void settingsUpdatedCb(UAVObjEvent * objEv);

static float accelKi = 0;
static float accelKp = 0;
static float yawBiasRate = 0;
static float gyroGain = 0.42;
static int16_t accelbias[3];
static float R[3][3];
static int8_t rotate = 0;
static bool zero_during_arming = false;

// These values are initialized by settings but can be updated by the attitude algorithm
static bool bias_correct_gyro = true;
static float gyro_bias[3] = {0,0,0};

/**
 * API for sensor fusion algorithms:
 * Configure(xQueueHandle gyro, xQueueHandle accel, xQueueHandle mag, xQueueHandle baro)
 *   Stores all the queues the algorithm will pull data from
 * FinalizeSensors() -- before saving the sensors modifies them based on internal state (gyro bias)
 * Update() -- queries queues and updates the attitude estiamte
 */

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AttitudeStart(void)
{
	// Create the queues for the sensors
	gyroQueue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(GyrosData));
	accelQueue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(AccelsData));
	magQueue = xQueueCreate(SENSOR_QUEUE_SIZE, sizeof(MagnetometerData));

	// Start main task
	xTaskCreate(SensorTask, (signed char *)"Sensors", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &sensorTaskHandle);
	xTaskCreate(AttitudeTask, (signed char *)"Attitude", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &attitudeTaskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_SENSORS, sensorTaskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_ATTITUDE, attitudeTaskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_ATTITUDE);
	PIOS_WDG_RegisterFlag(PIOS_WDG_SENSORS);

	return 0;
}

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t AttitudeInitialize(void)
{
	AttitudeActualInitialize();
	GyrosInitialize();
	AccelsInitialize();
	MagnetometerInitialize();
	AttitudeSettingsInitialize();
	
	// Initialize quaternion
	AttitudeActualData attitude;
	AttitudeActualGet(&attitude);
	attitude.q1 = 1;
	attitude.q2 = 0;
	attitude.q3 = 0;
	attitude.q4 = 0;
	AttitudeActualSet(&attitude);

	// Cannot trust the values to init right above if BL runs
	gyro_bias[0] = 0;
	gyro_bias[1] = 0;
	gyro_bias[2] = 0;

	for(uint8_t i = 0; i < 3; i++)
		for(uint8_t j = 0; j < 3; j++)
			R[i][j] = 0;

	AttitudeSettingsConnectCallback(&settingsUpdatedCb);

	return 0;
}

MODULE_INITCALL(AttitudeInitialize, AttitudeStart)

int32_t accel_test;
int32_t gyro_test;
int32_t mag_test;
//int32_t pressure_test;

/**
 * The sensor task.  This polls the gyros at 500 Hz and pumps that data to
 * stabilization and to the attitude loop
 */
static void SensorTask(void *parameters)
{
	uint8_t init = 0;
	portTickType lastSysTime;

	AlarmsClear(SYSTEMALARMS_ALARM_SENSORS);

	accel_test = PIOS_BMA180_Test();
	gyro_test = PIOS_MPU6000_Test();
	mag_test = PIOS_HMC5883_Test();
	
	if(accel_test < 0 || gyro_test < 0 || mag_test < 0) {
		AlarmsSet(SYSTEMALARMS_ALARM_SENSORS, SYSTEMALARMS_ALARM_CRITICAL);
		while(1) {
			PIOS_WDG_UpdateFlag(PIOS_WDG_SENSORS);
			vTaskDelay(1);
		}
	}
	
	// Main task loop
	lastSysTime = xTaskGetTickCount();
	while (1) {
	
		// TODO: This initialization stuff should be refactored from here
		FlightStatusData flightStatus;
		FlightStatusGet(&flightStatus);
		
		if((xTaskGetTickCount() < 7000) && (xTaskGetTickCount() > 1000)) {
			// For first 7 seconds use accels to get gyro bias
			accelKp = 1;
			accelKi = 0.9;
			yawBiasRate = 0.23;
			init = 0;
		}
		else if (zero_during_arming && (flightStatus.Armed == FLIGHTSTATUS_ARMED_ARMING)) {
			accelKp = 1;
			accelKi = 0.9;
			yawBiasRate = 0.23;
			init = 0;
		} else if (init == 0) {
			// Reload settings (all the rates)
			AttitudeSettingsAccelKiGet(&accelKi);
			AttitudeSettingsAccelKpGet(&accelKp);
			AttitudeSettingsYawBiasRateGet(&yawBiasRate);
			init = 1;
		}
	
		
		if(updateSensors() != 0)
			AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_ERROR);
		else {
			// TODO: Push data onto queue 
			AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);
		}

		PIOS_WDG_UpdateFlag(PIOS_WDG_SENSORS);
		vTaskDelayUntil(&lastSysTime, 2 / portTICK_RATE_MS);
	}
}

/**
 * Module thread, should not return.
 */
static void AttitudeTask(void *parameters)
{
	uint8_t init = 0;
	AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);

	// Force settings update to make sure rotation loaded
	settingsUpdatedCb(AttitudeSettingsHandle());
		
	// Main task loop
	while (1) {
	
		// This  function blocks on data queue
		updateAttitudeComplimentary();
		
		PIOS_WDG_UpdateFlag(PIOS_WDG_ATTITUDE);
	}
}


uint32_t accel_samples;
uint32_t gyro_samples;
struct pios_bma180_data accel;
struct pios_mpu6000_data gyro;
int32_t accel_accum[3] = {0, 0, 0};
int32_t gyro_accum[3] = {0,0,0};
float scaling;

/**
 * Get an update from the sensors
 * @param[in] attitudeRaw Populate the UAVO instead of saving right here
 * @return 0 if successfull, -1 if not
 */
static int32_t updateSensors()
{
	int32_t read_good;
	int32_t count;
	
	for (int i = 0; i < 3; i++) {
		accel_accum[i] = 0;
		gyro_accum[i] = 0;
	}
	accel_samples = 0;
	gyro_samples = 0;
	
	// Make sure we get one sample
	count = 0;
	while((read_good = PIOS_BMA180_ReadFifo(&accel)) != 0);
	while(read_good == 0) {	
		count++;

		accel_accum[0] += accel.x;
		accel_accum[1] += accel.y;
		accel_accum[2] += accel.z;

		read_good = PIOS_BMA180_ReadFifo(&accel);
	}
	accel_samples = count;
	
	float accels[3] = {(float) accel_accum[1] / accel_samples, (float) accel_accum[0] / accel_samples, -(float) accel_accum[2] / accel_samples};

	// Not the swaping of channel orders
	scaling = PIOS_BMA180_GetScale();
	AccelsData accelsData; // Skip get as we set all the fields
	accelsData.x = (accels[0] - accelbias[0]) * scaling;
	accelsData.y = (accels[1] - accelbias[1]) * scaling;
	accelsData.z = (accels[2] - accelbias[2]) * scaling;
	accelsData.temperature = 25.0f + ((float) accel.temperature - 2.0f) / 2.0f;
	AccelsSet(&accelsData);
	
	// Push the data onto the queue for attitude to consume
	if(xQueueSendToBack(accelQueue, (void *) &accelsData, 0) != pdTRUE) {
		AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_WARNING);
	}
	
	// Make sure we get one sample
	count = 0;
	while((read_good = PIOS_MPU6000_ReadFifo(&gyro)) != 0);
	while(read_good == 0) {
		count++;
		
		gyro_accum[0] += gyro.gyro_x;
		gyro_accum[1] += gyro.gyro_y;
		gyro_accum[2] += gyro.gyro_z;
		
		read_good = PIOS_MPU6000_ReadFifo(&gyro);
	}
	gyro_samples = count;

	float gyros[3] = {(float) gyro_accum[1] / gyro_samples, (float) gyro_accum[0] / gyro_samples, -(float) gyro_accum[2] / gyro_samples};
	
	scaling = PIOS_MPU6000_GetScale();
	GyrosData gyrosData; // Skip get as we set all the fields
	gyrosData.x = gyros[0] * scaling;
	gyrosData.y = gyros[1] * scaling;
	gyrosData.z = gyros[2] * scaling;
	gyrosData.temperature = 35.0f + ((float) gyro.temperature + 512.0f) / 340.0f;
	// Don't set yet.  We push raw data to queue but then bias correct for other modules
	
	// Push the data onto the queue for attitude to consume
	if(xQueueSendToBack(gyroQueue, (void *) &gyrosData, 0) != pdTRUE) {
		AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_WARNING);
	}
		
	// Apply bias correction to the gyros
	if(bias_correct_gyro) {
		gyrosData.x += gyro_bias[0];
		gyrosData.y += gyro_bias[1];
		gyrosData.z += gyro_bias[2];
	}
	GyrosSet(&gyrosData);

	// Because most crafts wont get enough information from gravity to zero yaw gyro, we try
	// and make it average zero (weakly)
	gyro_bias[2] += - gyrosData.z * yawBiasRate;

	if (PIOS_HMC5883_NewDataAvailable()) {
		int16_t values[3];
		PIOS_HMC5883_ReadMag(values);
		MagnetometerData mag; // Skip get as we set all the fields
		mag.x = -values[0];
		mag.y = -values[1];
		mag.z = -values[2];
		MagnetometerSet(&mag);
	}
	
	return 0;
}

float accel_mag;
float qmag;
static int32_t updateAttitudeComplimentary()
{
	GyrosData gyrosData;
	AccelsData accelsData;
	
	if(xQueueReceive(gyroQueue, (void *) &gyrosData, 10 / portTICK_RATE_MS) != pdTRUE ||
	   xQueueReceive(accelQueue, (void *) &accelsData, 10 / portTICK_RATE_MS) != pdTRUE) {
		AlarmsSet(SYSTEMALARMS_ALARM_ATTITUDE, SYSTEMALARMS_ALARM_ERROR);
		return -1;
	}

	static int32_t timeval;
	float dT = PIOS_DELAY_DiffuS(timeval) / 1000000.0f;
	timeval = PIOS_DELAY_GetRaw();
	
	float q[4];
	
	AttitudeActualData attitudeActual;
	AttitudeActualGet(&attitudeActual);

	float grot[3];
	float accel_err[3];
	
	// Get the current attitude estimate
	quat_copy(&attitudeActual.q1, q);
	
	// Rotate gravity to body frame and cross with accels
	grot[0] = -(2 * (q[1] * q[3] - q[0] * q[2]));
	grot[1] = -(2 * (q[2] * q[3] + q[0] * q[1]));
	grot[2] = -(q[0] * q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3]);
	CrossProduct((const float *) &accelsData.x, (const float *) grot, accel_err);

	// Account for accel magnitude
	accel_mag = accelsData.x*accelsData.x + accelsData.y*accelsData.y + accelsData.z*accelsData.z;
	accel_mag = sqrtf(accel_mag);
	accel_err[0] /= accel_mag;
	accel_err[1] /= accel_mag;
	accel_err[2] /= accel_mag;	
	
	// Accumulate integral of error.  Scale here so that units are (deg/s) but Ki has units of s
	gyro_bias[0] += accel_err[0] * accelKi;
	gyro_bias[1] += accel_err[1] * accelKi;
		
	// Correct rates based on error, integral component dealt with in updateSensors
	gyrosData.x += accel_err[0] * accelKp / dT;
	gyrosData.y += accel_err[1] * accelKp / dT;
	gyrosData.z += accel_err[2] * accelKp / dT;


	// Work out time derivative from INSAlgo writeup
	// Also accounts for the fact that gyros are in deg/s
	float qdot[4];
	qdot[0] = (-q[1] * gyrosData.x - q[2] * gyrosData.y - q[3] * gyrosData.z) * dT * F_PI / 180 / 2;
	qdot[1] = (q[0] * gyrosData.x - q[3] * gyrosData.y + q[2] * gyrosData.z) * dT * F_PI / 180 / 2;
	qdot[2] = (q[3] * gyrosData.x + q[0] * gyrosData.y - q[1] * gyrosData.z) * dT * F_PI / 180 / 2;
	qdot[3] = (-q[2] * gyrosData.x + q[1] * gyrosData.y + q[0] * gyrosData.z) * dT * F_PI / 180 / 2;
	
	// Take a time step
	q[0] = q[0] + qdot[0];
	q[1] = q[1] + qdot[1];
	q[2] = q[2] + qdot[2];
	q[3] = q[3] + qdot[3];
	
	if(q[0] < 0) {
		q[0] = -q[0];
		q[1] = -q[1];
		q[2] = -q[2];
		q[3] = -q[3];
	}
	
	// Renomalize
	qmag = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
	q[0] = q[0] / qmag;
	q[1] = q[1] / qmag;
	q[2] = q[2] / qmag;
	q[3] = q[3] / qmag;

	// If quaternion has become inappropriately short or is nan reinit.
	// THIS SHOULD NEVER ACTUALLY HAPPEN
	if((fabs(qmag) < 1.0e-3f) || (qmag != qmag)) {
		q[0] = 1;
		q[1] = 0;
		q[2] = 0;
		q[3] = 0;
	}

	quat_copy(q, &attitudeActual.q1);

	// Convert into eueler degrees (makes assumptions about RPY order)
	Quaternion2RPY(&attitudeActual.q1,&attitudeActual.Roll);

	AttitudeActualSet(&attitudeActual);
	
	AlarmsClear(SYSTEMALARMS_ALARM_ATTITUDE);

	return 0;
}

static void settingsUpdatedCb(UAVObjEvent * objEv) {
	AttitudeSettingsData attitudeSettings;
	AttitudeSettingsGet(&attitudeSettings);


	accelKp = attitudeSettings.AccelKp;
	accelKi = attitudeSettings.AccelKi;
	yawBiasRate = attitudeSettings.YawBiasRate;
	gyroGain = attitudeSettings.GyroGain;

	zero_during_arming = attitudeSettings.ZeroDuringArming == ATTITUDESETTINGS_ZERODURINGARMING_TRUE;
	bias_correct_gyro = attitudeSettings.BiasCorrectGyro == ATTITUDESETTINGS_BIASCORRECTGYRO_TRUE;

	accelbias[0] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_X];
	accelbias[1] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_Y];
	accelbias[2] = attitudeSettings.AccelBias[ATTITUDESETTINGS_ACCELBIAS_Z];

	gyro_bias[0] = attitudeSettings.GyroBias[ATTITUDESETTINGS_GYROBIAS_X] / 100.0f;
	gyro_bias[1] = attitudeSettings.GyroBias[ATTITUDESETTINGS_GYROBIAS_Y] / 100.0f;
	gyro_bias[2] = attitudeSettings.GyroBias[ATTITUDESETTINGS_GYROBIAS_Z] / 100.0f;

	// Indicates not to expend cycles on rotation
	if(attitudeSettings.BoardRotation[0] == 0 && attitudeSettings.BoardRotation[1] == 0 &&
	   attitudeSettings.BoardRotation[2] == 0) {
		rotate = 0;

		// Shouldn't be used but to be safe
		float rotationQuat[4] = {1,0,0,0};
		Quaternion2R(rotationQuat, R);
	} else {
		float rotationQuat[4];
		const float rpy[3] = {attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_ROLL],
			attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_PITCH],
			attitudeSettings.BoardRotation[ATTITUDESETTINGS_BOARDROTATION_YAW]};
		RPY2Quaternion(rpy, rotationQuat);
		Quaternion2R(rotationQuat, R);
		rotate = 1;
	}
}
/**
  * @}
  * @}
  */