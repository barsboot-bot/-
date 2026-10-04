class CfgPatches
{
    class Unessed_Sounds
    {
        units[] = {};
        weapons[] = {};
        requiredVersion = 0.1;
        requiredAddons[] = {"DZ_Sounds", "Unessed_Core"};
        author = "KRa Tos";
        version = "1.0.0";
        fileName = "Unessed_Sounds.pbo";
    };
};

class CfgSoundShaders
{
    class Unessed_Cassette_Shader
    {
        position[] = {0,0,0};
        volume = 1.0;
        range = 50;
        attenuation = "LinearAttenuation";
        soundSet[] = {"Unessed_Cassette_SoundSet"};
    };

    class Unessed_Radio_Shader
    {
        position[] = {0,0,0};
        volume = 1.0;
        range = 100;
        attenuation = "LinearAttenuation";
        soundSet[] = {"Unessed_Radio_SoundSet"};
    };
};

class CfgSoundSets
{
    class Unessed_Cassette_SoundSet
    {
        soundShaders[] = {"Unessed_Cassette_Shader"};
    };

    class Unessed_Radio_SoundSet
    {
        soundShaders[] = {"Unessed_Radio_Shader"};
    };
};
