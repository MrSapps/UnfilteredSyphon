#include "common.h"

void debug(void) {
}

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80011FE0);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInit);

INCLUDE_ASM("asm/main/nonmatchings/particle", particleGroupAdd);

INCLUDE_ASM("asm/main/nonmatchings/particle", particleGroupDelete);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFree);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGroupFree);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticlePoolAllocate);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitEmitterPoint);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitEmitterMask);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitEmitterXZPlane);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitAngle);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitLifeColorNoInfrared);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitLifeColor);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitLifeColorFade);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitDirection);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitNegativeDirection);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStart);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleInitBreath);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleBreath);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDamageDelay);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTaserRetract);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTaserSetTarget);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTaserGetDestination);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTaser);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateTaser);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleTracer);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012000);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012010);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFootSplash);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitGobj);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitFlakJacket);

INCLUDE_ASM("asm/main/nonmatchings/particle", iParticleExplosion);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleExplosion);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSmoke);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFire);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFindGroup);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireShow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireHide);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSmokeFade);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireFade);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleNPCFireShow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleNPCFireHide);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleNPCFireFade);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleNPCFireMove);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGrenadeLauncher);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateGrenadeLauncher);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFireWaveDeath);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitWall);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFuse);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitSnow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitWallChopper);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleHitChopper);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleFindFloor);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSparkShower);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSparkShowerTime);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSparkSphere);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleWood);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleSparkFence);

void ParticleElectricalSparkShow(void) {
}

void ParticleElectricalSparkHide(void) {
}

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleElectricalSpark);

void ParticleGunFlash(void) {
}

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012030);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPoint);

INCLUDE_ASM("asm/main/nonmatchings/particle", particleGlassTexture);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleBuildGlass);

INCLUDE_ASM("asm/main/nonmatchings/particle", func_80052D90);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPaneWall);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGlassPaneCeiling);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStartRain);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStartSnow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleStopSnow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGroupUpdateWind);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleGroupUpdateRain);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateFragmentMeterFrictionFloor);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateExplosion);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyFlat4);

INCLUDE_RODATA("asm/main/nonmatchings/particle", D_80012050);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateGlass);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayRain);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateSnow);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyWorld);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateMovement);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateLineMeter);

INCLUDE_ASM("asm/main/nonmatchings/particle", getShift);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleDisplayPolyFlat);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateTracer);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateShell);

INCLUDE_ASM("asm/main/nonmatchings/particle", ssUpdateAndDisplay);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdateFireLight);

INCLUDE_ASM("asm/main/nonmatchings/particle", ParticleUpdate);
