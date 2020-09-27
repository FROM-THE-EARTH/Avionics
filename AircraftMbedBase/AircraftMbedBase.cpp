#include "AircraftMbedBase.h"
#include "Utils.h"
#include "Nature.h"

bool AircraftMbedBase::initialize()
{
  // IM920
  transmitter_->initialize();
  transmitter_->transmit("Initializing");

  receiver_->initialize();
  if (receiver_->isAvailable())
  {
    receiver_->attach(this, &AircraftMbedBase::onReceive);
  }

  // LSM9DS1
  lsm_->initialize();
  lsm_->setGyroScale(LSM9DS1::gyro_scale::G_SCALE_500DPS);

  // LPS331
  lps_->initialize();
  lps_->setResolution(LPS331_I2C_PRESSURE_AVG_512, LPS331_I2C_TEMP_AVG_128);
  lps_->setDataRate(LPS331_I2C_DATARATE_25HZ);

  // SD
  /*fp = fopen("/sd/data.csv", "w");
  if(fp != NULL) {
      fprintf(fp,"time,temperature,pressure,accX,accY,accZ,gyroX,gyroY,gyroZ,magnX,magnY,magnZ,longitude,latitude\r\n");
  }*/

  // GPS

  transmitter_->transmit("Initialized");

  //initialize datas
  datas.maxAltitude = -1000.0f;
  datas.bootTime = 0.0f;
  
  //initialize time
  bootTime = Kernel::Clock::now();
  nowTime = bootTime;
  preTime = nowTime;

  return isReady();
}

void AircraftMbedBase::update()
{
  nowTime = Kernel::Clock::now();

  datas.time = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - bootTime).count() * 0.000001f;

  datas.deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - preTime).count() * 0.000001f;

  preTime = nowTime;

}

bool AircraftMbedBase::isReady(bool showDetail)
{
  bool allModulesAvailable = true;

  allModulesAvailable &= transmitter_->isAvailable();
  allModulesAvailable &= receiver_->isAvailable();
  allModulesAvailable &= lps_->isAvailable();
  allModulesAvailable &= lsm_->isAvailable();

  if (showDetail)
  {
    transmitter_->transmit(transmitter_->status());
    transmitter_->transmit(receiver_->status());
    transmitter_->transmit(lps_->status());
    transmitter_->transmit(lsm_->status());
  }
  transmitter_->transmit("Modules: " + std::string(allModulesAvailable ? "OK" : "NG"));

  return allModulesAvailable;
}

void AircraftMbedBase::end()
{
  // end processing
}

void AircraftMbedBase::waiting()
{
  beginRecord();
}

void AircraftMbedBase::waitingLaunch()
{
  if (launchCondition())
  {
    datas.launchTime = datas.time;
    beginRecord();
    scene = Scene::InFlight;
  }
}

void AircraftMbedBase::inFlight()
{
  if (detachCondition())
  {
    datas.detachTime = datas.time;
    detachAircraft();
  }

  if (decelerationCondition())
  {
    datas.decelerationTime = datas.time;
    openParachute();
  }

  if (landingCondition())
  {
    datas.landingTime = datas.time;
    endRecord();
    scene = Scene::Landing;
  }
}

void AircraftMbedBase::landing()
{
  // if(gps.isAvailable)
  // transmit gps info
}

void AircraftMbedBase::getDatas()
{
  lsm_->readAccel();
  lsm_->readGyro();
  lsm_->readMag();

  datas.pressure = lps_->getPressure();
  datas.temperature = lps_->getTemperature();

  datas.accel = Vec3(lsm_->ax, lsm_->ay, lsm_->az);
  datas.gyro = Vec3(lsm_->gx, lsm_->gy, lsm_->gz);
  datas.magn = Vec3(lsm_->mx, lsm_->my, lsm_->mz);

  datas.altitude =
      Utils::calcAltitude(basePressure, datas.pressure, datas.temperature);

  if (datas.altitude > datas.maxAltitude)
  {
    datas.maxAltitude = datas.altitude;
  }

  // datas.longitude;
  // datas.latitude;
}

void AircraftMbedBase::writeDatas()
{
  transmitter_->transmit("("+std::to_string(datas.roll)+", "+std::to_string(datas.pitch)+", "+std::to_string(datas.yaw)+")");
}

void AircraftMbedBase::onReceive()
{
  switch (checkCommand(receiver_->receive()))
  {
  case Commands::ResetMbed:
    __NVIC_SystemReset();
    break;

  case Commands::EscapePreparing:
    scene = Scene::ReadyToLaunch;
    break;

  case Commands::CheckSensors:
    isReady(true);
    break;

  case Commands::ClosingServo:
    // Close servo
    break;

  case Commands::None:
    transmitter_->transmit("Invalid command");
    break;
  }
}