/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "odometry_display.h"
#include "rviz/visualization_manager.h"
#include "rviz/properties/property.h"
#include "rviz/properties/property_manager.h"
#include "rviz/common.h"

#include "ogre_tools/arrow.h"

#include <tf/transform_listener.h>

#include <boost/bind.hpp>

#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSceneManager.h>

namespace rviz
{

OdometryDisplay::OdometryDisplay( const std::string& name, VisualizationManager* manager )
: Display( name, manager )
, color_( 1.0f, 0.1f, 0.0f )
, keep_(100)
, position_tolerance_( 0.1 )
, angle_tolerance_( 0.1 )
, tf_filter_(*manager->getTFClient(), "", 5, update_nh_)
{
  scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();

  tf_filter_.connectInput(sub_);
  tf_filter_.registerCallback(boost::bind(&OdometryDisplay::incomingMessage, this, _1));
}

OdometryDisplay::~OdometryDisplay()
{
  unsubscribe();

  clear();
}

void OdometryDisplay::clear()
{
  D_Arrow::iterator it = arrows_.begin();
  D_Arrow::iterator end = arrows_.end();
  for ( ; it != end; ++it )
  {
    delete *it;
  }
  arrows_.clear();

  if (last_used_message_)
  {
    last_used_message_.reset();
  }

  tf_filter_.clear();
}

void OdometryDisplay::setTopic( const std::string& topic )
{
  unsubscribe();
  topic_ = topic;
  subscribe();

  propertyChanged(topic_property_);

  causeRender();
}

void OdometryDisplay::setColor( const Color& color )
{
  color_ = color;

  D_Arrow::iterator it = arrows_.begin();
  D_Arrow::iterator end = arrows_.end();
  for ( ; it != end; ++it )
  {
    ogre_tools::Arrow* arrow = *it;
    arrow->setColor( color.r_, color.g_, color.b_, 1.0f );
  }

  propertyChanged(color_property_);

  causeRender();
}

void OdometryDisplay::setKeep(uint32_t keep)
{
  keep_ = keep;

  propertyChanged(keep_property_);
}

void OdometryDisplay::setPositionTolerance( float tol )
{
  position_tolerance_ = tol;

  propertyChanged(position_tolerance_property_);
}

void OdometryDisplay::setAngleTolerance( float tol )
{
  angle_tolerance_ = tol;

  propertyChanged(angle_tolerance_property_);
}

void OdometryDisplay::subscribe()
{
  if ( !isEnabled() )
  {
    return;
  }

  sub_.subscribe(update_nh_, topic_, 5);
}

void OdometryDisplay::unsubscribe()
{
  sub_.unsubscribe();
}

void OdometryDisplay::onEnable()
{
  scene_node_->setVisible( true );
  subscribe();
}

void OdometryDisplay::onDisable()
{
  unsubscribe();
  clear();
  scene_node_->setVisible( false );
}

void OdometryDisplay::createProperties()
{
  color_property_ = property_manager_->createProperty<ColorProperty>( "Color", property_prefix_, boost::bind( &OdometryDisplay::getColor, this ),
                                                                          boost::bind( &OdometryDisplay::setColor, this, _1 ), parent_category_, this );
  topic_property_ = property_manager_->createProperty<ROSTopicStringProperty>( "Topic", property_prefix_, boost::bind( &OdometryDisplay::getTopic, this ),
                                                                                boost::bind( &OdometryDisplay::setTopic, this, _1 ), parent_category_, this );
  ROSTopicStringPropertyPtr topic_prop = topic_property_.lock();
  topic_prop->setMessageType(nav_msgs::Odometry::__s_getDataType());

  position_tolerance_property_ = property_manager_->createProperty<FloatProperty>( "Position Tolerance", property_prefix_, boost::bind( &OdometryDisplay::getPositionTolerance, this ),
                                                                               boost::bind( &OdometryDisplay::setPositionTolerance, this, _1 ), parent_category_, this );
  angle_tolerance_property_ = property_manager_->createProperty<FloatProperty>( "Angle Tolerance", property_prefix_, boost::bind( &OdometryDisplay::getAngleTolerance, this ),
                                                                                 boost::bind( &OdometryDisplay::setAngleTolerance, this, _1 ), parent_category_, this );

  keep_property_ = property_manager_->createProperty<IntProperty>( "Keep", property_prefix_, boost::bind( &OdometryDisplay::getKeep, this ),
                                                                               boost::bind( &OdometryDisplay::setKeep, this, _1 ), parent_category_, this );
}

void OdometryDisplay::processMessage( const nav_msgs::Odometry::ConstPtr& message )
{
  if ( last_used_message_ )
  {
    Ogre::Vector3 last_position(last_used_message_->pose.pose.position.x, last_used_message_->pose.pose.position.y, last_used_message_->pose.pose.position.z);
    Ogre::Vector3 current_position(message->pose.pose.position.x, message->pose.pose.position.y, message->pose.pose.position.z);
    Ogre::Quaternion last_orientation(last_used_message_->pose.pose.orientation.w, last_used_message_->pose.pose.orientation.x, last_used_message_->pose.pose.orientation.y, last_used_message_->pose.pose.orientation.z);
    Ogre::Quaternion current_orientation(message->pose.pose.orientation.w, message->pose.pose.orientation.x, message->pose.pose.orientation.y, message->pose.pose.orientation.z);

    if ((last_position - current_position).length() < position_tolerance_ && (last_orientation - current_orientation).normalise() < angle_tolerance_)
    {
      return;
    }
  }

  ogre_tools::Arrow* arrow = new ogre_tools::Arrow( scene_manager_, scene_node_, 0.8f, 0.05f, 0.2f, 0.2f );

  transformArrow( message, arrow );

  arrow->setColor( color_.r_, color_.g_, color_.b_, 1.0f );
  arrow->setUserData( Ogre::Any((void*)this) );

  arrows_.push_back( arrow );
  last_used_message_ = message;
}

void OdometryDisplay::transformArrow( const nav_msgs::Odometry::ConstPtr& message, ogre_tools::Arrow* arrow )
{
  std::string frame_id = message->header.frame_id;
  if ( frame_id.empty() )
  {
    frame_id = fixed_frame_;
  }

  btQuaternion bt_q;
  tf::quaternionMsgToTF(message->pose.pose.orientation, bt_q);
  tf::Stamped<tf::Pose> pose( btTransform( bt_q, btVector3( message->pose.pose.position.x, message->pose.pose.position.y, message->pose.pose.position.z ) ),
                              message->header.stamp, frame_id );

  try
  {
    vis_manager_->getTFClient()->transformPose( fixed_frame_, pose, pose );
  }
  catch(tf::TransformException& e)
  {
    ROS_ERROR( "Error transforming 2d base pose '%s' from frame '%s' to frame '%s'\n", name_.c_str(), message->header.frame_id.c_str(), fixed_frame_.c_str() );
  }

  btScalar yaw, pitch, roll;
  pose.getBasis().getEulerZYX( yaw, pitch, roll );
  Ogre::Matrix3 orient;
  orient.FromEulerAnglesZXY( Ogre::Radian( roll ), Ogre::Radian( pitch ), Ogre::Radian( yaw ) );
  arrow->setOrientation( orient );

  Ogre::Vector3 pos( pose.getOrigin().x(), pose.getOrigin().y(), pose.getOrigin().z() );
  robotToOgre( pos );
  arrow->setPosition( pos );
}

void OdometryDisplay::targetFrameChanged()
{
}

void OdometryDisplay::fixedFrameChanged()
{
  tf_filter_.setTargetFrame( fixed_frame_ );
  clear();
}

void OdometryDisplay::update(float wall_dt, float ros_dt)
{
  if (keep_ > 0)
  {
    while (arrows_.size() > keep_)
    {
      delete arrows_.front();
      arrows_.pop_front();
    }
  }
}

void OdometryDisplay::incomingMessage( const nav_msgs::Odometry::ConstPtr& message )
{
  processMessage(message);
  causeRender();
}

void OdometryDisplay::reset()
{
  clear();
}

} // namespace rviz