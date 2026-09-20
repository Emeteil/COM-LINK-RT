#include "test_globals.h"

namespace TestSupport
{
    FakeChannel& Channel()
    {
        static FakeChannel channel;
        return channel;
    }
}

ComLinkRTProtocol::ProtocolHandler protocol(TestSupport::Channel());
