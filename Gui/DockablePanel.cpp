//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 *Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 *contact: immarespond at gmail dot com
 *
 */

#include "DockablePanel.h"

#include <iostream>
#include <QLayout>
#include <QAction>
#include <QTabWidget>
#include <QStyle>
#include <QUndoStack>
#include <QUndoCommand>
#include <QToolTip>

#include "Global/AppManager.h"

#include "Engine/Node.h"
#include "Engine/Project.h"
#include "Engine/Knob.h"
#include "Engine/EffectInstance.h"

#include "Gui/NodeGui.h"
#include "Gui/KnobGui.h"
#include "Gui/KnobGuiTypes.h" // for Group_KnobGui
#include "Gui/KnobGuiFactory.h"
#include "Gui/LineEdit.h"
#include "Gui/Button.h"
#include "Gui/NodeGraph.h"

using std::make_pair;
using namespace Natron;

DockablePanel::DockablePanel(KnobHolder* holder
                             , QVBoxLayout* container
                             , HeaderMode headerMode
                             , const QString& initialName
                             , const QString& helpToolTip
                             , bool createDefaultTab, const QString& defaultTab
                             , QWidget *parent)
:QFrame(parent)
,_container(container)
,_mainLayout(NULL)
,_headerWidget(NULL)
,_headerLayout(NULL)
,_nameLineEdit(NULL)
,_nameLabel(NULL)
,_tabWidget(NULL)
,_helpButton(NULL)
,_minimize(NULL)
,_cross(NULL)
,_undoButton(NULL)
,_redoButton(NULL)
,_minimized(false)
,_undoStack(new QUndoStack)
,_knobs()
,_holder(holder)
,_tabs()
,_defaultTabName(defaultTab)
{
    _mainLayout = new QVBoxLayout(this);
    _mainLayout->setSpacing(0);
    _mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(_mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFrameShape(QFrame::Box);
    
    if(headerMode != NO_HEADER){
        
        _headerWidget = new QFrame(this);
        _headerWidget->setFrameShape(QFrame::Box);
        _headerLayout = new QHBoxLayout(_headerWidget);
        _headerLayout->setContentsMargins(0, 0, 0, 0);
        _headerLayout->setSpacing(0);
        _headerWidget->setLayout(_headerLayout);
        
        
        QPixmap pixHelp ;
        appPTR->getIcon(NATRON_PIXMAP_HELP_WIDGET,&pixHelp);
        _helpButton = new Button(QIcon(pixHelp),"",_headerWidget);
        
        QString tooltip = helpToolTip;
        int countSinceLastNewLine = 0;
        for (int i = 0; i < tooltip.size();++i) {
            if(tooltip.at(i) != QChar('\n')){
                ++countSinceLastNewLine;
            }else{
                countSinceLastNewLine = 0;
            }
            if (countSinceLastNewLine%80 == 0 && i!=0) {
                /*Find closest word end and insert a new line*/
                while(i < tooltip.size() && tooltip.at(i)!=QChar(' ')){
                    ++i;
                }
                tooltip.insert(i, QChar('\n'));
            }
        }
        _helpButton->setToolTip(tooltip);
        _helpButton->setFixedSize(15, 15);
        QObject::connect(_helpButton, SIGNAL(clicked()), this, SLOT(showHelp()));
        
        
        QPixmap pixM;
        appPTR->getIcon(NATRON_PIXMAP_MINIMIZE_WIDGET,&pixM);
        
        QPixmap pixC;
        appPTR->getIcon(NATRON_PIXMAP_CLOSE_WIDGET,&pixC);
        _minimize=new Button(QIcon(pixM),"",_headerWidget);
        _minimize->setFixedSize(15,15);
        _minimize->setCheckable(true);
        QObject::connect(_minimize,SIGNAL(toggled(bool)),this,SLOT(minimizeOrMaximize(bool)));
        _cross=new Button(QIcon(pixC),"",_headerWidget);
        _cross->setFixedSize(15,15);
        QObject::connect(_cross,SIGNAL(clicked()),this,SLOT(close()));
        
        
        QPixmap pixUndo ;
        appPTR->getIcon(NATRON_PIXMAP_UNDO,&pixUndo);
        QPixmap pixUndo_gray ;
        appPTR->getIcon(NATRON_PIXMAP_UNDO_GRAYSCALE,&pixUndo_gray);
        QIcon icUndo;
        icUndo.addPixmap(pixUndo,QIcon::Normal);
        icUndo.addPixmap(pixUndo_gray,QIcon::Disabled);
        _undoButton = new Button(icUndo,"",_headerWidget);
        _undoButton->setToolTip("Undo the last change made to this operator");
        _undoButton->setEnabled(false);
        
        QPixmap pixRedo ;
        appPTR->getIcon(NATRON_PIXMAP_REDO,&pixRedo);
        QPixmap pixRedo_gray;
        appPTR->getIcon(NATRON_PIXMAP_REDO_GRAYSCALE,&pixRedo_gray);
        QIcon icRedo;
        icRedo.addPixmap(pixRedo,QIcon::Normal);
        icRedo.addPixmap(pixRedo_gray,QIcon::Disabled);
        _redoButton = new Button(icRedo,"",_headerWidget);
        _redoButton->setToolTip("Redo the last change undone to this operator");
        _redoButton->setEnabled(false);
        
        
        QObject::connect(_undoButton, SIGNAL(pressed()),this, SLOT(onUndoPressed()));
        QObject::connect(_redoButton, SIGNAL(pressed()),this, SLOT(onRedoPressed()));
        
        if(headerMode != READ_ONLY_NAME){
            _nameLineEdit = new LineEdit(_headerWidget);
            _nameLineEdit->setText(initialName);
            QObject::connect(_nameLineEdit,SIGNAL(textEdited(QString)),this,SIGNAL(nameChanged(QString)));
            _headerLayout->addWidget(_nameLineEdit);
        }else{
            _nameLabel = new QLabel(initialName,_headerWidget);
            _headerLayout->addWidget(_nameLabel);
        }
        
        _headerLayout->addStretch();
        
        _headerLayout->addWidget(_undoButton);
        _headerLayout->addWidget(_redoButton);
        
        _headerLayout->addStretch();
        _headerLayout->addWidget(_helpButton);
        _headerLayout->addWidget(_minimize);
        _headerLayout->addWidget(_cross);
        
        _mainLayout->addWidget(_headerWidget);
        
    }
    
    _tabWidget = new QTabWidget(this);
    _mainLayout->addWidget(_tabWidget);
    
    if(createDefaultTab){
        addTab(_defaultTabName);
    }
}

DockablePanel::~DockablePanel(){
    delete _undoStack;
    
    ///Delete the knob gui if they weren't before
    ///normally the onKnobDeletion() function should have cleared them
    for(std::map<boost::shared_ptr<Knob>,KnobGui*>::const_iterator it = _knobs.begin();it!=_knobs.end();++it){
        if(it->second){
            delete it->second;
        }
    }
}

void DockablePanel::initializeKnobs(){
   
    /// function called to create the gui for each knob. It can be called several times in a row
    /// without any damage
    
    const std::vector<boost::shared_ptr<Knob> >& knobs = _holder->getKnobs();
    
    for(U32 i = 0 ; i < knobs.size(); ++i){
        
        if(knobs[i]->typeName() == "Tab"){
            
            ///if the knob is a tab, look-up the tab widget to check whether the tab already exists or not
            
            bool found = false;
            QString tabName(knobs[i]->getDescription().c_str());
            for(int j = 0 ; j < _tabWidget->count(); ++j){
                if(_tabWidget->tabText(j) == tabName){
                    found = true;
                    break;
                }
            }
            if(!found){
                ///if it doesn't exist, create it
                addTab(tabName);
            }
        }else{
            
            ///this is not a tab knob
            
            KnobGui* gui = findKnobGuiOrCreate(knobs[i]);
            if(!gui){
                // this should happen for Custom Knobs, which have no GUI (only an interact)
                return;
            }
            
            ///if widgets for the KnobGui have already been created, don't do this
            if(!gui->hasWidgetBeenCreated()){
                
                
                ///find to what to belongs the knob. by default it belongs to the default tab.
                std::map<QString,std::pair<QWidget*,int> >::iterator parentTab = _tabs.end() ;
                
                
                boost::shared_ptr<Knob> parentKnob = knobs[i]->getParentKnob();
                
                ///if the parent is a tab find it
                if(parentKnob && parentKnob->typeName() == "Tab"){
                    //make sure the tab has been created;
                    (void)findKnobGuiOrCreate(parentKnob);
                    std::map<QString,std::pair<QWidget*,int> >::iterator it = _tabs.find(parentKnob->getDescription().c_str());
                    
                    ///if it crashes there's serious problem because findKnobGuiOrCreate(parentKnob) should have registered
                    ///the tab. Maybe you passed incorrect pointers ?
                    assert(it != _tabs.end());
                    parentTab = it;
                    
                }else{
                    ///defaults to the default tab
                    parentTab = _tabs.find(_defaultTabName);
                    ///The dockpanel must have a default tab if you didn't declare your own tab to
                    ///put your knobs into!
                    assert(parentTab != _tabs.end());
                }
                if(parentTab != _tabs.end()){
                    ++parentTab->second.second;
                }
                
                ///this might be wrong we don't want to use Qt's parenting !
                //gui->setParent(parentTab->second.first);
                
                ///if the knob has specified that it didn't want to trigger a new line, decrement the current row
                /// index of the tab
                ///FIXME: this is bugged because we don't tell to the createGui() function that it is going to need
                ///to put its widgets on the right of others, so it is going to corrupt the layout. Commenting for now.
//                if(!gui->triggerNewLine() && i!=0){
//                    --parentTab->second.second;
//                }
//                
                gui->createGUI(dynamic_cast<QGridLayout*>(parentTab->second.first->layout()),//< ptr to the grid layout
                               parentTab->second.second); //< the row index
                
                /// if this knob is within a group, check that the group is visible, i.e. the toplevel group is unfolded
                if (parentKnob && parentKnob->typeName() == "Group") {
                    
                    Group_KnobGui* parentGui = dynamic_cast<Group_KnobGui*>(findKnobGuiOrCreate(parentKnob));
                    assert(parentGui);
                    
                    
                    ///FIXME: this offsetColumn is never really used. Shall we use this anyway? It seems
                    ///to work fine without it.
                    int offsetColumn = knobs[i]->determineHierarchySize();
                    parentGui->addKnob(gui,parentTab->second.second,offsetColumn);
                    
                    
                    bool showit = true;
                    // see KnobGui::setSecret() for a very similar code
                    while (showit && parentKnob && parentKnob->typeName() == "Group") {
                        assert(parentGui);
                        // check for secretness and visibility of the group
                        if (parentKnob->isSecret() || !parentGui->isChecked()) {
                            showit = false; // one of the including groups is folder, so this item is hidden
                        }
                        // prepare for next loop iteration
                        parentKnob = parentKnob->getParentKnob();
                        if (parentKnob) {
                            parentGui = dynamic_cast<Group_KnobGui*>(findKnobGuiOrCreate(parentKnob));
                        }
                    }
                    if (showit) {
                        gui->show();
                    } else {
                        //gui->hide(); // already hidden? please comment if it's not.
                    }
                }
                
                
            }
            
        }
        
    }
}


void DockablePanel::addTab(const QString& name){
    QWidget* newTab = new QWidget(_tabWidget);
    QGridLayout *tabLayout = new QGridLayout(newTab);
    newTab->setLayout(tabLayout);
    tabLayout->setVerticalSpacing(2);
    tabLayout->setContentsMargins(3, 0, 0, 0);
    tabLayout->setHorizontalSpacing(5);
    
    _tabWidget->addTab(newTab,name);
    _tabs.insert(make_pair(name,make_pair(newTab,0)));
}

const QUndoCommand* DockablePanel::getLastUndoCommand() const{
        return _undoStack->command(_undoStack->index()-1);
}

void DockablePanel::pushUndoCommand(QUndoCommand* cmd){
    _undoStack->setActive();
    _undoStack->push(cmd);
    if(_undoButton && _redoButton){
        _undoButton->setEnabled(_undoStack->canUndo());
        _redoButton->setEnabled(_undoStack->canRedo());
    }
}

void DockablePanel::onUndoPressed(){
    _undoStack->undo();
    if(_undoButton && _redoButton){
        _undoButton->setEnabled(_undoStack->canUndo());
        _redoButton->setEnabled(_undoStack->canRedo());
    }
    emit undoneChange();
}

void DockablePanel::onRedoPressed(){
    _undoStack->redo();
    if(_undoButton && _redoButton){
        _undoButton->setEnabled(_undoStack->canUndo());
        _redoButton->setEnabled(_undoStack->canRedo());
    }
    emit redoneChange();
}

void DockablePanel::showHelp(){
    QToolTip::showText(QCursor::pos(), _helpButton->toolTip());
}

void DockablePanel::closePanel(){
    
    setVisible(false);
    
    std::vector<QWidget*> _panels;
    for(int i =0 ; i < _container->count(); ++i) {
        if (QWidget *myItem = dynamic_cast <QWidget*>(_container->itemAt(i))){
            _panels.push_back(myItem);
            _container->removeWidget(myItem);
        }
    }
    for (U32 i =0 ; i < _panels.size(); ++i) {
        _container->addWidget(_panels[i]);
    }
    
    update();
    emit closed();
}
void DockablePanel::minimizeOrMaximize(bool toggled){
    _minimized=toggled;
    if(_minimized){
        QPixmap pixM;
        appPTR->getIcon(NATRON_PIXMAP_MAXIMIZE_WIDGET,&pixM);
        _minimize->setIcon(QIcon(pixM));
        emit minimized();
    }else{
        QPixmap pixM;
        appPTR->getIcon(NATRON_PIXMAP_MINIMIZE_WIDGET,&pixM);
        _minimize->setIcon(QIcon(pixM));
        emit maximized();
    }
    _tabWidget->setVisible(!_minimized);
    std::vector<QWidget*> _panels;
    for(int i =0 ; i < _container->count(); ++i) {
        if (QWidget *myItem = dynamic_cast <QWidget*>(_container->itemAt(i))){
            _panels.push_back(myItem);
            _container->removeWidget(myItem);
        }
    }
    for (U32 i =0 ; i < _panels.size(); ++i) {
        _container->addWidget(_panels[i]);
    }
    update();
}


void DockablePanel::onNameChanged(const QString& str){
    if(_nameLabel){
        _nameLabel->setText(str);
    }else if(_nameLineEdit){
        _nameLineEdit->setText(str);
    }
}


Button* DockablePanel::insertHeaderButton(int headerPosition){
    Button* ret = new Button(_headerWidget);
    _headerLayout->insertWidget(headerPosition, ret);
    return ret;
}

KnobGui* DockablePanel::findKnobGuiOrCreate(boost::shared_ptr<Knob> knob) {
    assert(knob);
    for (std::map<boost::shared_ptr<Knob>,KnobGui*>::const_iterator it = _knobs.begin(); it!=_knobs.end(); ++it) {
        if(it->first == knob){
            return it->second;
        }
    }
    
    QObject::connect(knob.get(),SIGNAL(deleted(Knob*)),this,SLOT(onKnobDeletion(Knob*)));
    
    KnobGui* ret =  appPTR->getKnobGuiFactory().createGuiForKnob(knob,this);
    if(!ret && knob->typeName() != "Tab"){
        // this should happen for Custom Knobs, which have no GUI (only an interact)
        // this should happen for tab Knobs, which have no GUI
        std::cout << "Failed to create gui for Knob " << knob->getName() << " of type " << knob->typeName() << std::endl;
        return NULL;
    }
    _knobs.insert(make_pair(knob, ret));
    return ret;
    
}

void DockablePanel::onKnobDeletion(Knob* k){
    for(std::map<boost::shared_ptr<Knob>,KnobGui*>::iterator it = _knobs.begin();it!=_knobs.end();++it){
        if (it->first.get() == k) {
            if(it->second){
                delete it->second;
            }
            _knobs.erase(it);
            return;
        }
    }
}





NodeSettingsPanel::NodeSettingsPanel(NodeGui* NodeUi ,QVBoxLayout* container,QWidget *parent)
:DockablePanel(NodeUi->getNode()->getLiveInstance(),
               container,
               DockablePanel::FULLY_FEATURED,
               NodeUi->getNode()->getName().c_str(),
               NodeUi->getNode()->description().c_str(),
               true,
               "Settings",
               parent)
,_nodeGUI(NodeUi)
{}



void NodeSettingsPanel::setSelected(bool s){
    _selected = s;
    style()->unpolish(this);
    style()->polish(this);
}


