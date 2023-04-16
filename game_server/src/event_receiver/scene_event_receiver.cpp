#include "scene_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SceneEventReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
dispatcher.sink<S2CEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive4>();
}

void SceneEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive4>();
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

