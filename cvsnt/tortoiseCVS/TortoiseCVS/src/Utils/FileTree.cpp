// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - December 2003

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


#include "StdAfx.h"
#include "FileTree.h"
#include "TortoiseDebug.h"
#include "PathUtils.h"

//////////////////////////////////////////////////////////////////////////////
// FileTree

// Constructor
FileTree::FileTree()
   : m_rootNode(0, 0, "root", 0)
{
}


// Destructor 
FileTree::~FileTree()
{
}


// Add a node
FileTree::Node* FileTree::AddNode(const NodeName& path, NodeType type, UserData* userData)
{
   return m_rootNode.AddNode(path, type, userData);
}


// Calculate node ids
void FileTree::CalcNodeIds()
{
   int currentId = 0;
   CalcNodeId(&m_rootNode, currentId);
}



// Calculate a node id
void FileTree::CalcNodeId(Node* node, int& currentId)
{
   node->SetId(currentId);
   currentId++;
   // Process all subnodes
   if (node->GetNodeType() == TYPE_DIR)
   {
      DirNode* dirNode = (DirNode*) node;
      NodeIterator it = dirNode->Begin();
      while (it != dirNode->End())
      {
         CalcNodeId(it->second, currentId);
         it++;
      }
   }
   node->SetLastChildId(currentId - 1);
}


//////////////////////////////////////////////////////////////////////////////
// FileTree::Node

// Constructor
FileTree::Node::Node(FileTree* fileTree, Node* parent, const NodeName& name, 
                     UserData* userData)
   : m_FileTree(fileTree), m_Name(name), m_Parent(parent), 
     m_UserData(userData), m_NodeType(TYPE_NODE) 
{
}


// Destructor
FileTree::Node::~Node() 
{
   if (m_UserData)
   {
      delete m_UserData;
   }
}


// Get path
std::string FileTree::Node::GetPath() const
{
   if (m_Parent)
   {
      return EnsureTrailingDelimiter(m_Parent->GetPath()) + m_Parent->GetName();
   }
   else
   {
      return "";
   }
}


// Get path
std::string FileTree::Node::GetPathFrom(const Node* node) const
{
   if (m_Parent == node || m_Parent == 0)
   {
      return GetName();
   }
   else
   {
      return EnsureTrailingDelimiter(m_Parent->GetPathFrom(node)) + GetName();
   }
}


// Get full name
std::string FileTree::Node::GetFullName() const
{
   return EnsureTrailingDelimiter(this->GetPath()) + GetName();
}



//////////////////////////////////////////////////////////////////////////////
// FileTree::FileNode

FileTree::FileNode::FileNode(FileTree* fileTree, Node* parent, 
                             const NodeName& name, UserData* userData)
   : Node(fileTree, parent, name, userData)
{
   m_NodeType = TYPE_FILE;
}



//////////////////////////////////////////////////////////////////////////////
// FileTree::DirNode

// Constructor
FileTree::DirNode::DirNode(FileTree* fileTree, Node* parent, 
                           const NodeName& name, UserData* userData)
   : Node(fileTree, parent, name, userData) 
{
   m_NodeType = TYPE_DIR;
}


// Destructor
FileTree::DirNode::~DirNode()
{
   // Delete subnodes
   NodeIterator it = m_Nodes.begin();
   while (it != m_Nodes.end())
   {
      delete it->second;
      it++;
   }
}


// Add a node
FileTree::Node* FileTree::DirNode::AddNode(const std::string &path, NodeType type, 
   UserData* userData)
{
   std::string mypath = path;
   std::string currentName = CutFirstPathElement(mypath);
   if (mypath.empty())
   {
      // Node must not exist
      ASSERT(GetNode(currentName) == 0);

      // Add the node
      Node* node;
      if (type == TYPE_DIR)
      {
         node = new DirNode(m_FileTree, this, currentName, userData);
      }
      else
      {
         node = new FileNode(m_FileTree, this, currentName, userData);
      }
      m_Nodes[currentName] = node;
      return node;
   }
   else
   {
      // Check if the node exists already
      Node* node = GetNode(currentName);
      ASSERT(node == 0 || node->GetNodeType() == TYPE_DIR);
      if (node == 0)
      {
         // Add directory node
         node = new DirNode(m_FileTree, this, currentName, 0);
         m_Nodes[currentName] = node;
      }
      return ((DirNode*) node)->AddNode(mypath, type, userData);
   }
}



// Get all subnodes
void FileTree::DirNode::GetSubNodes(NodeList& nodeList, NodeTypes nodeTypes)
{
   // Get all my nodes
   NodeMap::iterator it = m_Nodes.begin();
   FileTree::Node* node;
   while (it != m_Nodes.end())
   {
      node = it->second;
      if (node->GetNodeType() & nodeTypes)
      {
         nodeList.push_back(node);
      }

      if (node->GetNodeType() == TYPE_DIR)
      {
         ((DirNode *) node)->GetSubNodes(nodeList, nodeTypes);
      }
      it++;
   }


}



