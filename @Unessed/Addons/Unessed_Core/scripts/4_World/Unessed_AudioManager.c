class UnessedAudioManager
{
    static void PlaySoundAtLocation(string soundSet, vector position, float volume = 1.0)
    {
        SoundObject soundObj = GameSoundManager.CreateSoundObject(soundSet);
        if (soundObj)
        {
            soundObj.SetPosition(position);
            soundObj.SetVolume(volume);
            soundObj.SoundPlay();
        }
    }

    static void StopSoundAtLocation(vector position)
    {
        // Логика остановки звука
        GameSoundManager.StopSoundAtPosition(position);
    }

    static float CalculateVolume(vector sourcePos, vector listenerPos)
    {
        float distance = vector.Distance(sourcePos, listenerPos);
        if (distance >= UnessedConstants.MAX_HEARING_DISTANCE)
            return 0.0;

        return 1.0 - (distance / UnessedConstants.MAX_HEARING_DISTANCE);
    }
};
