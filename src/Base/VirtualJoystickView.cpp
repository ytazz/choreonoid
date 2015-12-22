/**
   \file
   \author Shin'ichiro Nakaoka
*/

#include "VirtualJoystickView.h"
#include <cnoid/ViewManager>
#include <cnoid/Buttons>
#include <cnoid/ExtJoystick>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QKeyEvent>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "gettext.h"

using namespace std;
using namespace boost;
using namespace cnoid;

namespace {

const bool TRACE_FUNCTIONS = false;

enum {
    ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT,
    L_AXIS_UP, L_AXIS_DOWN, L_AXIS_LEFT, L_AXIS_RIGHT,
    R_AXIS_UP, R_AXIS_DOWN, R_AXIS_LEFT, R_AXIS_RIGHT,
    BUTTON_A, BUTTON_B, BUTTON_X, BUTTON_Y,
    NUM_JOYSTICK_ELEMENTS
};

struct ButtonInfo {
    const char* label;
    int row;
    int column;
    bool isAxis;
    double activeValue;
    int id;
    int key;
};

ButtonInfo buttonInfo[] = {

    { "^", 0, 1, true, -1.0, 7, Qt::Key_Up },
    { "v", 2, 1, true,  1.0, 7, Qt::Key_Down },
    { "<", 1, 0, true, -1.0, 6, Qt::Key_Left },
    { ">", 1, 2, true,  1.0, 6, Qt::Key_Right },

    { "E", 3, 3, true, -1.0, 1, Qt::Key_E },
    { "D", 5, 3, true,  1.0, 1, Qt::Key_D },
    { "S", 4, 2, true, -1.0, 0, Qt::Key_S },
    { "F", 4, 4, true,  1.0, 0, Qt::Key_F },

    { "I", 3, 8, true, -1.0, 4, Qt::Key_I },
    { "K", 5, 8, true,  1.0, 4, Qt::Key_K },
    { "J", 4, 7, true, -1.0, 3, Qt::Key_J },
    { "L", 4, 9, true,  1.0, 3, Qt::Key_L },

    { "A", 2, 10, false, 1.0, 0, Qt::Key_A },
    { "B", 1, 11, false, 1.0, 1, Qt::Key_B },
    { "X", 1,  9, false, 1.0, 2, Qt::Key_X },
    { "Y", 0, 10, false, 1.0, 3, Qt::Key_Y }
};
    
}

namespace cnoid {

class VirtualJoystickViewImpl : public ExtJoystick
{
public:
    VirtualJoystickView* self;
    QGridLayout grid;
    ToolButton buttons[NUM_JOYSTICK_ELEMENTS];
    typedef std::map<int, int> KeyToButtonMap;
    KeyToButtonMap keyToButtonMap;
    vector<double> keyValues;
    Signal<void(int id, bool isPressed)> sigButton_;
    Signal<void(int id, double position)> sigAxis_;
    boost::mutex mutex;
    vector<double> axisPositions;
    vector<bool> buttonStates;

    VirtualJoystickViewImpl(VirtualJoystickView* self);
    ~VirtualJoystickViewImpl();
    void onKeyStateChanged(int key, bool on);

    virtual int numAxes() const;
    virtual int numButtons() const;
    virtual bool readCurrentState();
    virtual double getPosition(int axis) const;
    virtual bool getButtonState(int button) const;
    virtual bool isActive() const;
    virtual SignalProxy<void(int id, bool isPressed)> sigButton();
    virtual SignalProxy<void(int id, double position)> sigAxis();
};

}


void VirtualJoystickView::initializeClass(ExtensionManager* ext)
{
    ext->viewManager().registerClass<VirtualJoystickView>(
        "VirtualJoystickView", N_("Joystick"), ViewManager::SINGLE_OPTIONAL);
}


VirtualJoystickView::VirtualJoystickView()
{
    impl = new VirtualJoystickViewImpl(this);
}


VirtualJoystickViewImpl::VirtualJoystickViewImpl(VirtualJoystickView* self)
    : self(self),
      keyValues(NUM_JOYSTICK_ELEMENTS, 0.0)
{
    self->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    self->setDefaultLayoutArea(View::BOTTOM);
    self->setFocusPolicy(Qt::WheelFocus);
    
    QVBoxLayout* vbox = new QVBoxLayout;

    for(int i=0; i < NUM_JOYSTICK_ELEMENTS; ++i){
        ButtonInfo& info = buttonInfo[i];
        buttons[i].setText(info.label);
        grid.addWidget(&buttons[i], info.row, info.column);
        keyToButtonMap[info.key] = i;
        if(info.isAxis){
            if(info.id >= axisPositions.size()){
                axisPositions.resize(info.id + 1, 0.0);
            }
        } else {
            if(info.id >= buttonStates.size()){
                buttonStates.resize(info.id + 1, false);
            }
        }
    }

    vbox->addLayout(&grid);
    self->setLayout(vbox);

    ExtJoystick::registerJoystick("VirtualJoystickView", this);
}


VirtualJoystickView::~VirtualJoystickView()
{
    delete impl;
}


VirtualJoystickViewImpl::~VirtualJoystickViewImpl()
{

}


void VirtualJoystickView::keyPressEvent(QKeyEvent* event)
{
    impl->onKeyStateChanged(event->key(), true);
}


void VirtualJoystickView::keyReleaseEvent(QKeyEvent* event)
{
    impl->onKeyStateChanged(event->key(), false);
}


void VirtualJoystickViewImpl::onKeyStateChanged(int key, bool on)
{
    KeyToButtonMap::iterator p = keyToButtonMap.find(key);
    if(p != keyToButtonMap.end()){
        int index = p->second;
        ToolButton& button = buttons[index];
        ButtonInfo& info = buttonInfo[p->second];
        button.setDown(on);
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            keyValues[index] = on ? info.activeValue : 0.0;
        }
    }
}


int VirtualJoystickViewImpl::numAxes() const
{
    return axisPositions.size();
}


int VirtualJoystickViewImpl::numButtons() const
{
    return buttonStates.size();
}


bool VirtualJoystickViewImpl::readCurrentState()
{
    std::fill(axisPositions.begin(), axisPositions.end(), 0.0);

    {
        boost::unique_lock<boost::mutex> lock(mutex);
        for(int i=0; i < NUM_JOYSTICK_ELEMENTS; ++i){
            ButtonInfo& info = buttonInfo[i];
            if(info.isAxis){
                axisPositions[info.id] += keyValues[i];
            } else {
                buttonStates[info.id] = keyValues[i];
            }
        }
    }
    return true;
}


double VirtualJoystickViewImpl::getPosition(int axis) const
{
    if(axis >=0 && axis < axisPositions.size()){
        return axisPositions[axis];
    }
    return 0.0;
}


bool VirtualJoystickViewImpl::getButtonState(int button) const
{
    if(button >= 0 && button < buttonStates.size()){
        return buttonStates[button];
    }
    return false;
}


bool VirtualJoystickViewImpl::isActive() const
{
    for(size_t i=0; i < axisPositions.size(); ++i){
        if(axisPositions[i] != 0.0){
            return true;
        }
    }
    for(size_t i=0; i < buttonStates.size(); ++i){
        if(buttonStates[i]){
            return true;
        }
    }
    return false;
}


SignalProxy<void(int id, bool isPressed)> VirtualJoystickViewImpl::sigButton()
{
    return sigButton_;
}


SignalProxy<void(int id, double position)> VirtualJoystickViewImpl::sigAxis()
{
    return sigAxis_;
}


bool VirtualJoystickView::storeState(Archive& archive)
{
    return true;
}


bool VirtualJoystickView::restoreState(const Archive& archive)
{
    return true;
}