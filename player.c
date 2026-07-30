#include <math.h>

#include "defs.h"

void initPlayer(Player *p) {

  /* DEFAULT SPAWN LOCATION */

  p->x   = 300;
  p->y   = 300;
  p->a   = 0;

  /* BASELINE VELOCITY */

  p->vx  = 0;
  p->vy  = 0;
  p->va  = 0;

  /* PLAYER DEFAULT CONFIG */

  p->speed       = 1.00;
  p->friction    = 0.85;
  p->turnSpeed   = 0.03;
  p->dashTimer   = 0;
  p->zOffset     = 0;
  p->fov         = 70.0;
  p->targetFov   = 70.0;
  p->pitch       = 0;
  p->idleTimer   = 0;
  p->bobAmp      = 0.015;

}

void updatePlayer(Player *p) {

  /* DASH COOLDOWN */

  if (p->dashTimer > 0) {

    p->dashTimer -= 1;
    p->targetFov = 85.0;

    if (p->dashTimer > 30) {

      p->zOffset = (35.0 - p->dashTimer) * 4.8;

    } else {

      p->zOffset = (p->dashTimer / 30.0) * 24.0;

    }

  } else {

    p->zOffset = 0;

    if (keys[bindForward] || keys[bindBackwards] ||
        keys[bindStrafeLeft] || keys[bindStrafeRight]) {

      p->targetFov = 78.0;

    }

    else {

      p->targetFov = 70.0;

    }

  }

  p->fov += (p->targetFov - p->fov) * 0.15;

  if (keys[bindLookDown]) p->pitch -= 4.0;
  if (keys[bindLookUp]) p->pitch += 4.0;
  if (keys[bindCenterView]) p->pitch = 0;

  if (p->pitch > 120) p->pitch = 120;
  if (p->pitch < -120) p->pitch = -120;

  if (fabs(p->vx) > 0.1 || fabs(p->vy) > 0.1) {

	p->idleTimer += 0.15;

	p->bobAmp += (0.030 - p->bobAmp) * 0.1;

  } else {

	p->idleTimer += 0.05;

	p->bobAmp += (0.015 - p->bobAmp) * 0.1;

  }

  /* KEY RESPONSES */

  if (keys[bindForward]) { p->vx += cos(p->a) * p->speed;
                     p->vy += sin(p->a) * p->speed; }

  if (keys[bindBackwards]) { p->vx -= cos(p->a) * p->speed;
                     p->vy -= sin(p->a) * p->speed; }

  if (keys[bindTurnLeft]) { p->va -= p->turnSpeed; }

  if (keys[bindTurnRight]) { p->va += p->turnSpeed; }

  if (keys[bindStrafeLeft]) {

	p->vx += cos(p->a - (PI / 2.0)) * p->speed;
	p->vy += sin(p->a - (PI / 2.0)) * p->speed;

  }

  if (keys[bindStrafeRight]) {

	p->vx += cos(p->a + (PI / 2.0)) * p->speed;
	p->vy += sin(p->a + (PI / 2.0)) * p->speed;

  }

  /* DASH LOGIC */

  if (p->dashTimer <= 0) {

	  if (keys[bindDash]) {

			if (keys[bindForward]) {

			p->vx += cos(p->a) * 12.0;
			p->vy += sin(p->a) * 12.0;
			p->dashTimer = 35;

			} else if (keys[bindBackwards]) {

			p->vx -= cos(p->a) * 12.0;
			p->vy -= sin(p->a) * 12.0;
			p->dashTimer = 35;

			} else if (keys[bindStrafeLeft]) {

			p->vx += cos(p->a - (PI / 2.0)) * 12.0;
			p->vy += sin(p->a - (PI / 2.0)) * 12.0;
			p->dashTimer = 35;

			} else if (keys[bindStrafeRight]) {

			p->vx += cos(p->a + (PI / 2.0)) * 12.0;
			p->vy += sin(p->a + (PI / 2.0)) * 12.0;
			p->dashTimer = 35;


			}

			else {

				p->vx += cos(p->a) * 12.0;
				p->vy += sin(p->a) * 12.0;
				p->dashTimer = 35;



			}

	  }

  }

  applyPhysics(p);

}
