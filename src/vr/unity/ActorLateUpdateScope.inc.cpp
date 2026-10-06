    struct ActorLateUpdateScope {
        void* actor;
        explicit ActorLateUpdateScope(void* value) : actor(value) {
            gakumas::vr::TraceLiveGazeStage(actor, nullptr, "actor-late-before");
        }
        ~ActorLateUpdateScope() {
            gakumas::vr::TickNaturalLiveGaze(actor);
            gakumas::vr::TraceLiveGazeStage(actor, nullptr, "actor-late-after");
            GakumasLocal::ModLoader::DispatchLateUpdate(actor, 0.0f);
        }
    };
