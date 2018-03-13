/**
   @author Japan Atomic Energy Agency
*/

#include <TrafficControlPluginHeader.h>

using namespace std;
using namespace cnoid;

TrafficControlShare* TrafficControlShare::_inst = 0;

TrafficControlShare::TrafficControlShare()
{

}

TrafficControlShare::~TrafficControlShare()
{

}

TrafficControlShare*
TrafficControlShare::instance()
{
    if( _inst == 0 ){
        _inst = new TrafficControlShare();
    }
    return _inst;
}

bool
TrafficControlShare::initialize()
{
    return true;
}

void
TrafficControlShare::finalize()
{

}

void TrafficControlShare::setTcsInstance(TrafficControlSimulatorItem* value)
{
    _val = value;
}

TrafficControlSimulatorItem* TrafficControlShare::getTcsInstance() const
{
    return _val;
}

void TrafficControlShare::setTcsRunning(bool value)
{
    _isRunning = value;
}

bool TrafficControlShare::getTcsRunning() const
{
    return _isRunning;
}
