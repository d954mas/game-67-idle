#include "box3d/box3d.h"
#include <math.h>
#include <stdio.h>
#ifndef FEATURE_BOX3D
#error Missing feature compile definition
#endif
#define CHECK(value) do { if (!(value)) { fprintf(stderr,"Failed: %s\n",#value); return 1; } } while (0)
int main(void) {
    b3WorldDef def=b3DefaultWorldDef();
    def.gravity=(b3Vec3){0,-10,0};
    b3WorldId world=b3CreateWorld(&def);
    b3WorldId other=b3CreateWorld(&def);
    b3BodyDef body=b3DefaultBodyDef();
    body.position=(b3Pos){0,-.5f,0};
    b3BodyId floor=b3CreateBody(world,&body);
    b3ShapeDef shape=b3DefaultShapeDef();
    b3BoxHull hull=b3MakeBoxHull(5,.5f,5);
    b3CreateHullShape(floor,&shape,&hull.base);
    body.type=b3_dynamicBody;body.position=(b3Pos){0,3,0};
    b3BodyId ball=b3CreateBody(world,&body);
    b3Sphere sphere={.radius=.5f};
    b3CreateSphereShape(ball,&shape,&sphere);
    b3BodyId untouched=b3CreateBody(other,&body);
    b3CreateSphereShape(untouched,&shape,&sphere);
    for(int i=0;i<180;i++)b3World_Step(world,1.0f/60.0f,4);
    b3Pos pos=b3Body_GetPosition(ball);
    CHECK(isfinite(pos.y)&&pos.y>.4f&&pos.y<.65f);
    CHECK(b3Body_GetPosition(untouched).y==3);
    b3DestroyWorld(world);b3DestroyWorld(other);
    puts("Feature physics: collision, stepping, independent worlds passed");
    return 0;
}
