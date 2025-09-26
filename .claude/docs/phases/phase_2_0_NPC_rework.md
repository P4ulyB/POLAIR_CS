
GOAL: The goal of this is to create a basic object spawner that uses the object pooling system.
I'd like to attach the PACS_SpawnManager data asset to the Game mode (or wherever you best decide) which contains an association between 
the 'Spawn Classes' and the projects Blueprints. Once that association is made - i'd like to place the spawner in the level, select the 'Spawn Class',
hit play and the object pooling system populates the map with the desired assets. 

CONTEXT:

The reason why i asked for these files to be deleted in my initial request this session, was because these classes were littered with dependencies and hard references. 
This was my fault for not validating steps incrementally when building.
I'd like to rebuild some of these components from scratch and i'd like to start with a 'spawn manager' subsystem. 

I recall that the spawner subsystem was responsible for holding data regarding NPC types and settings.
The types of 'spawnable classes' should be as follows...

Human.Police
Human.QAS
Human.QFRS
Human.Civ
Human.POI

Veh.Police
Veh.QAS
Veh.QFRS
Veh.Civ
Veh.VOI

Env.Fire
Env.Smoke

Res.1
Res.2
Res.3
Res.4
Res.5

I recall there was a data asset that we placed in our Game Mode class. The intention of the data asset was to assign
Blueprint assets to the 'spawnable classes'.

The third part was the spawner, where you would place a spawner in the map, select the 'Spawnable Class' (eg. Human.Police), then on begin play, it would spawn
that blueprint at that location. 

The fourth part was the Spawn Pool. this was designed specifically to be an optimisation for use in a multiplayer dedicated server environment using the
object pooling system. 

These systems in principle existed in this project but it was never correctly implemented. 

I'd like you to attempt to implement it again. 

CONSTRAINTS: 

1. No hard references 
2. Use smart pointers where the benefit is clear 
3. Use interfaces when necessary 
4. Create and use a blueprint function library in C++ to keep code tidy and avoid repeating code when the benefit is clear