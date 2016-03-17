#include "PlayState.h"
#include "PauseState.h"

#include "Shapes/OgreBulletCollisionsConvexHullShape.h"
#include "Shapes/OgreBulletCollisionsTrimeshShape.h"    
#include "Utils/OgreBulletCollisionsMeshToShapeConverter.h"
#include "Shapes/OgreBulletCollisionsSphereShape.h"
#include "OgreBulletCollisionsRay.h"

using namespace std;

template<> PlayState* Ogre::Singleton<PlayState>::msSingleton = 0;

Vector3 _now;
Vector3 _next;
Vector3 _despCamera;
std::vector<Vector3> _points;

int _speedCam = 10;
Vector3 _vnCam;

//Para parar en una zona y pasar a la siguiente---
int _contZone = 1500;
bool _nextZone = false;
//-------------------------------------------------

//Fuerza de las flechas----
float _force=0.0;
float _forceInc=0.2;
bool _mousePressed=false;
//-------------------------

void
PlayState::enter ()
{
  _root = Ogre::Root::getSingletonPtr();

  // Se recupera el gestor de escena y la cámara.----------------
  _sceneMgr = _root->getSceneManager("SceneManager");
  _camera = _sceneMgr->getCamera("IntroCamera");
  _viewport = _root->getAutoCreatedWindow()->addViewport(_camera);
  // Nuevo background colour.
  _viewport->setBackgroundColour(Ogre::ColourValue(0.0, 0.0, 0.0));
  //-------------------------------------

  //Pruebo a crear algo y ponerlo---------
  _sceneMgr->setAmbientLight(Ogre::ColourValue(0.8, 0.8, 0.8));
  //-------------------------------------
   
  //Camara--------------------
  _camera->setPosition(Ogre::Vector3(15,15,15));//el tercer parametro hace que se aleje mas la camara, el segundo para que rote hacia arriba o hacia abajo
  _camera->lookAt(Ogre::Vector3(0,0,0));//bajar el 60 un poco
  _camera->setNearClipDistance(5);
  _camera->setFarClipDistance(10000);
  //-----------------------------
  
  _numEntities = 0;    // Numero de Shapes instanciadas
  _timeLastObject = 0; // Tiempo desde que se añadio el ultimo objeto

  // Creacion del modulo de debug visual de Bullet ------------------
  _debugDrawer = new OgreBulletCollisions::DebugDrawer();
  _debugDrawer->setDrawWireframe(true);  
  SceneNode *node = _sceneMgr->getRootSceneNode()->createChildSceneNode("debugNode", Vector3::ZERO);
  node->attachObject(static_cast <SimpleRenderable *>(_debugDrawer));

  // Creacion del mundo (definicion de los limites y la gravedad) ---
  AxisAlignedBox worldBounds = AxisAlignedBox (Vector3 (-10000, -10000, -10000), Vector3 (10000,  10000,  10000));
  Vector3 gravity = Vector3(0, -9.8, 0);

  _world = new OgreBulletDynamics::DynamicsWorld(_sceneMgr,worldBounds, gravity);
  _world->setDebugDrawer (_debugDrawer);
  _world->setShowDebugShapes (true);  // Muestra los collision shapes

  // Creacion de los elementos iniciales del mundo
  CreateInitialWorld();
  _arrowSpeed=30.0;

  // Interfaz --------------------
  CEGUI::Window* sheet=CEGUI::System::getSingleton().getDefaultGUIContext().getRootWindow();
  sheet->getChildAtIdx(0)->setVisible(false);
  
  //Boton Salir Juego---------------------------------------
  CEGUI::Window* quitButton = CEGUI::WindowManager::getSingleton().createWindow("TaharezLook/Button","Ex1/QuitButton");
  quitButton->setText("Exit");
  quitButton->setSize(CEGUI::USize(CEGUI::UDim(0.15,0),CEGUI::UDim(0.05,0)));
  quitButton->setPosition(CEGUI::UVector2(CEGUI::UDim(0.05,0),CEGUI::UDim(0.8,0)));
  quitButton->subscribeEvent(CEGUI::PushButton::EventClicked,CEGUI::Event::Subscriber(&PlayState::quit,this));
  //-------------------------------------------------------
  
  sheet->addChild(quitButton);
  //------------------------------

  //Posicion de la camara, _now y _next
  _now=_camera->getPosition();
  _next=Vector3(-1,-1,-1);
  //-----------------------------------

  // Lista de puntos por lo que pasara la camara (Prueba)----
  
  Vector3 p1(30,15,15);
  Vector3 p2(60,15,60);
  Vector3 p3(30,15,50);

  _points.push_back(p1);
  _points.push_back(p2);
  _points.push_back(p3);
  _points.push_back(_camera->getPosition());

  //--------------------------------------------------------


  _exitGame = false;
}

void
PlayState::exit ()
{
  //Sago del estado------------------------------
  _sceneMgr->clearScene();
  _root->getAutoCreatedWindow()->removeAllViewports();
  //--------------------------------------------
}

void
PlayState::pause()
{
}

void
PlayState::resume()
{
  //Se restaura el background colour.------------------------------
  _viewport->setBackgroundColour(Ogre::ColourValue(0.0, 0.0, 0.0));
  //---------------------------------------------------------------
}

void 
PlayState::CreateInitialWorld() {
  
  // Creacion de la entidad y del SceneNode ------------------------
  Plane plane1(Vector3(0,1,0), 0);    // Normal y distancia
  MeshManager::getSingleton().createPlane("p1",
  ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, plane1,
  400, 400, 1, 1, true, 1, 20, 20, Vector3::UNIT_Z);
  SceneNode* node = _sceneMgr->createSceneNode("ground");
  Entity* groundEnt = _sceneMgr->createEntity("planeEnt", "p1");
  groundEnt->setMaterialName("Ground");
  node->attachObject(groundEnt);
  _sceneMgr->getRootSceneNode()->addChild(node);

  // Creamos forma de colision para el plano ----------------------- 
  OgreBulletCollisions::CollisionShape *Shape;
  Shape = new OgreBulletCollisions::StaticPlaneCollisionShape
    (Ogre::Vector3(0,1,0), 0);   // Vector normal y distancia
  OgreBulletDynamics::RigidBody *rigidBodyPlane = new 
    OgreBulletDynamics::RigidBody("rigidBodyPlane", _world);

  // Creamos la forma estatica (forma, Restitucion, Friccion) ------
  rigidBodyPlane->setStaticShape(Shape, 0.1, 0.8); 

  // Anadimos los objetos Shape y RigidBody ------------------------
  _shapes.push_back(Shape);  
  _bodies.push_back(rigidBodyPlane);

  //Añado una diana----------------
  AddStaticObject(Vector3(0,10,0));
  //-------------------------------

  //Añado otra diana---------------- 
  AddStaticObject(Vector3(15,10,0));
  //-------------------------------

}

void 
PlayState::AddStaticObject(Vector3 pos) {
  
  Entity* entAim = _sceneMgr->createEntity("Cylinder.mesh");
  SceneNode* nodeAim = _sceneMgr->createSceneNode("Target"+StringConverter::toString(_numEntities)+"SN");
  nodeAim->attachObject(entAim);
  _sceneMgr->getRootSceneNode()->addChild(nodeAim);
  
  cout << "Name" << nodeAim->getName() << endl;

  OgreBulletCollisions::StaticMeshToShapeConverter *trimeshConverter2 = new 
  OgreBulletCollisions::StaticMeshToShapeConverter(entAim);
 
  OgreBulletCollisions::TriangleMeshCollisionShape *Trimesh2 = trimeshConverter2->createTrimesh();

  OgreBulletDynamics::RigidBody *rigidObject2 = new 
    OgreBulletDynamics::RigidBody("RigidBodydiana"+StringConverter::toString(_numEntities), _world);
  rigidObject2->setShape(nodeAim, Trimesh2, 0.5, 0.5, 0,pos,//4º Posicion inicial 
       Quaternion::IDENTITY); //El 5º parametro es la gravedad

  delete trimeshConverter2;

  _numEntities++;

  // Anadimos los objetos a las deques---
  _shapes.push_back(Trimesh2);   
  _bodies.push_back(rigidObject2); 
  //-------------------------------------
  

}

void 
PlayState::AddDynamicObject() {
  _timeLastObject = 0.25;   // Segundos para anadir uno nuevo... 

  Vector3 position = (_camera->getDerivedPosition() + _camera->getDerivedDirection().normalisedCopy() * 10);
 
  Entity *entity = _sceneMgr->createEntity("Arrow" + StringConverter::toString(_numEntities), "arrow.mesh");
  SceneNode *node = _sceneMgr->getRootSceneNode()->createChildSceneNode("Arrow" + StringConverter::toString(_numEntities)+"SN");
  node->attachObject(entity);

  //Obtengo la rotacion de la camara para lanzar la flecha correctamente--------------------
  Vector3 aux=_camera->getDerivedDirection().normalisedCopy(); //Este es el vector direccion
  Vector3 src = node->getOrientation() * Ogre::Vector3::UNIT_X;//Punto donde aparece la flecha
  Quaternion quat = src.getRotationTo(aux);//Rotacion
  //------------------------------------------------------------------------------------------
  
  //Colision por ConvexHull----------------------------------------------

  OgreBulletCollisions::StaticMeshToShapeConverter *trimeshConverter = NULL;
  OgreBulletCollisions::CollisionShape *boxShape = NULL;
  trimeshConverter = new OgreBulletCollisions::StaticMeshToShapeConverter(entity);
  boxShape = trimeshConverter->createConvex();


  OgreBulletDynamics::RigidBody *rigidBox = new OgreBulletDynamics::RigidBody("rigidBodyArrow" + StringConverter::toString(_numEntities), _world);
  
  rigidBox->setShape(node, boxShape,0.5 , 0.5 ,5.0 , position ,quat );

  //Compruebo que a fuerza no sea mas que la fuerza maxima permitida---
  if(_force>50.0){
    _force=50.0;
  }
  cout << "Fuerza: "<< _force << endl;
  //-------------------------------------------------------------------

  rigidBox->setLinearVelocity(_camera->getDerivedDirection().normalisedCopy() * _force); 

  _numEntities++;

  // Anadimos los objetos a las deques---
  _shapes.push_back(boxShape);   
  _bodies.push_back(rigidBox); 
  //-------------------------------------
   

}

bool
PlayState::frameStarted
(const Ogre::FrameEvent& evt)
{ 
  _deltaT = evt.timeSinceLastFrame;

  _world->stepSimulation(_deltaT); // Actualizar simulacion Bullet
  _timeLastObject -= _deltaT;

  //Actualizacion de la camara--------------
  updateCameraPosition();
  //----------------------------------------

  //Si estoy pulsado el raton aumento la fueza--
  if(_mousePressed){
    _force+=_forceInc;
  }else{
    _force = 0.0;
  }
  //--------------------------------------------

  //Deteccion Colisones--------------------
  DetectCollisionAim();
  //----------------------------------------

  return true;
}

void PlayState::DetectCollisionAim() {
  
}
bool
PlayState::frameEnded
(const Ogre::FrameEvent& evt)
{

  _deltaT = evt.timeSinceLastFrame;
  _world->stepSimulation(_deltaT); // Actualizar simulacion Bullet

  //Salir del juego--
  if (_exitGame)
    return false;
  //-----------------
  return true;
}

void
PlayState::keyPressed
(const OIS::KeyEvent &e)
{
  // Tecla p --> PauseState.-------
  if (e.key == OIS::KC_P) {
    pushState(PauseState::getSingletonPtr());
  }
  //-----------------

  //Movimiento camara---------------
  Vector3 vt(0,0,0);
  Real tSpeed = 20.0;
  int _desp=5;
  if (e.key == OIS::KC_UP) {
    vt+=Vector3(0,0,-_desp);
  }
  if (e.key == OIS::KC_DOWN) {
    vt+=Vector3(0,0,_desp);
  }
  if (e.key == OIS::KC_LEFT) {
    vt+=Vector3(-_desp,0,0);
  }
  if (e.key == OIS::KC_RIGHT) {
    vt+=Vector3(_desp,0,0);
  }
  _camera->moveRelative(vt * _deltaT * tSpeed);
  //-------------------------------- 


  //CEGUI--------------------------
  CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyDown(static_cast<CEGUI::Key::Scan>(e.key));
  CEGUI::System::getSingleton().getDefaultGUIContext().injectChar(e.text);
  //-------------------------------
}

void
PlayState::keyReleased
(const OIS::KeyEvent &e)
{
  //Salgo del juego---------------
  if (e.key == OIS::KC_ESCAPE) {
    _exitGame = true;
  }
  //-------------------------------
  //CEGUI--------------------------
  CEGUI::System::getSingleton().getDefaultGUIContext().injectKeyUp(static_cast<CEGUI::Key::Scan>(e.key));
  //-------------------------------
}

void
PlayState::mouseMoved
(const OIS::MouseEvent &e)
{

  //Movimiento camara-----------------------------------------------------------------------------
  float rotx = e.state.X.rel * _deltaT * -1;
  float roty = e.state.Y.rel * _deltaT * -1;
  _camera->yaw(Radian(rotx));
  _camera->pitch(Radian(roty));
  //----------------------------------------------------------------------------------------------

  //CEGUI--------------------------
  CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseMove(e.state.X.rel, e.state.Y.rel);
  //-------------------------------
}

void
PlayState::mousePressed
(const OIS::MouseEvent &e, OIS::MouseButtonID id)
{
  //CEGUI--------------------------
  CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonDown(convertMouseButton(id));
  //-------------------------------
  
  //Pongo el raton a pulsado---
  _mousePressed=true;
  //---------------------------
  
}

void
PlayState::mouseReleased
(const OIS::MouseEvent &e, OIS::MouseButtonID id)
{
  //CEGUI--------------------------
  CEGUI::System::getSingleton().getDefaultGUIContext().injectMouseButtonUp(convertMouseButton(id));
  //-------------------------------

  //Pongo el raton a levantado y disparo---
  AddDynamicObject();
  _mousePressed=false;
  //---------------------------------------
}

PlayState*
PlayState::getSingletonPtr ()
{
return msSingleton;
}

PlayState&
PlayState::getSingleton ()
{ 
  assert(msSingleton);
  return *msSingleton;
}

CEGUI::MouseButton PlayState::convertMouseButton(OIS::MouseButtonID id)//METODOS DE CEGUI
{
  //CEGUI--------------------------
  CEGUI::MouseButton ceguiId;
  switch(id)
    {
    case OIS::MB_Left:
      ceguiId = CEGUI::LeftButton;
      break;
    case OIS::MB_Right:
      ceguiId = CEGUI::RightButton;
      break;
    case OIS::MB_Middle:
      ceguiId = CEGUI::MiddleButton;
      break;
    default:
      ceguiId = CEGUI::LeftButton;
    }
  return ceguiId;
  //------------------------------
}

bool
PlayState::quit(const CEGUI::EventArgs &e)
{
  _exitGame=true;
  return true;
}

void
PlayState::updateCameraPosition()
{
  //Actualizacion de la posicion---------------------------
  _camera->setPosition(_camera->getPosition()+_vnCam*_speedCam*_deltaT);
  //-------------------------------------------------------

  //Comprobacion de punto----------------------------------
  int _distance=_next.distance(_camera->getPosition());
  if(_distance==0){
    _speedCam=0;
    if(--_contZone<0){
      _contZone=1500;
      _nextZone=true;
    }
    if(_nextZone){
      _now=_next;
      _next=Vector3(-1,-1,-1);
      _nextZone=false;
    }
  }
  //-------------------------------------------------------

  //Siguiente Punto--------------------------------------
  if(_next==Vector3(-1,-1,-1)){
    _speedCam=10;
    _despCamera=Vector3(0,0,0);
    if(_points.size()>0){
      _next=_points.front();
      _points.erase(_points.begin());
      Vector3 _aux = _next-_now;
      _vnCam= _aux.normalisedCopy();
    }else{//Hemos llegado al ultimo punto (Se acabaria el recorrido )
      _speedCam=0;
    }
  }
}