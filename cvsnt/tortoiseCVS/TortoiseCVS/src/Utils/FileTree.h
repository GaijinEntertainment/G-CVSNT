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

#ifndef FILE_TREE_H
#define FILE_TREE_H

#include "FixCompilerBugs.h"

#include <string>
#include <map>
#include "StringUtils.h"



// Node in the file tree
class FileTree
{
public:
   class UserData;
   class Node;
   class File;
   class Dir;

   enum NodeType
   {
      TYPE_NODE = 1,
      TYPE_FILE = 2,
      TYPE_DIR = 4
   };

   typedef int NodeTypes;

   typedef std::string NodeName;
   typedef std::map<NodeName, Node*, less_nocase> NodeMap;
   typedef std::map<NodeName, Node*, less_nocase>::const_iterator NodeIterator;
   typedef std::vector<Node*> NodeList;

   
   //////////////////////////////////
   // User data
   class UserData
   {
   public:
      // Constructor 
      UserData() {};
      // Destructor 
      virtual ~UserData() {};
   };


   //////////////////////////////////
   // Node in the tree
   class Node
   {
   public:
      // Constructor
      Node(FileTree* fileTree, Node* parent, const NodeName& name, UserData* userData);
      // Destructor
      virtual ~Node();

      // Get file tree
      inline const FileTree* GetFileTree() const
      {
         return m_FileTree;
      }

      // Get user data
      inline const UserData* GetUserData() const
      {
         return m_UserData;
      }

      // Set user data
      inline void SetUserData(UserData* userData)
      {
         m_UserData = userData;
      }

      // Get nodetype
      inline const NodeType GetNodeType() const
      {
         return m_NodeType;
      }

      // Get name
      inline const NodeName& GetName() const
      {
         return m_Name;
      }

      // Set name
      inline void SetName(const NodeName& name)
      {
         m_Name = name;
      }

      // Get parent
      inline const Node* GetParent() const
      {
         return m_Parent;
      }

      // Get Id
      inline int GetId() const
      {
         return m_Id;
      }

      // Set Id
      inline void SetId(int id)
      {
         m_Id = id;
      }

      // Get last child id
      inline int GetLastChildId() const
      {
         return m_LastChildId;
      }

      // Set Id
      inline void SetLastChildId(int id)
      {
         m_LastChildId = id;
      }

      // Get path
      std::string GetPath() const;

      // Get path
      std::string GetPathFrom(const Node* node) const;

      // Get full name
      std::string GetFullName() const;

   protected:
      FileTree* m_FileTree;
      NodeName m_Name;
      Node* m_Parent;
      UserData* m_UserData;
      NodeType m_NodeType;
      int m_Id;
      int m_LastChildId;
   };


   //////////////////////////////////
   // File node in the tree
   class FileNode : public Node
   {
   public:
      // Constructor
      FileNode(FileTree* fileTree, Node* parent, const NodeName& name,
         UserData* userData);
   };
   
   //////////////////////////////////
   // Dir node in the tree
   class DirNode : public Node
   {
   public:
      // Constructor
      DirNode(FileTree* fileTree, Node* parent, const NodeName& name, 
         UserData* userData); 
      // Destructor
      ~DirNode();

      // Add a node
      Node* AddNode(const std::string &path, NodeType type, 
         UserData* userData = 0);

      // Get a node
      inline Node* GetNode(const NodeName& name) const
      {
         NodeMap::const_iterator it = m_Nodes.find(name);
         if (it == m_Nodes.end())
         {
            return 0;
         }
         else
         {
            return it->second;
         }
      }

      // Get node iterator
      inline const NodeIterator Begin() const
      {
         return m_Nodes.begin();
      }

      // Get name
      inline const NodeIterator End() const
      {
         return m_Nodes.end();
      }

      // Has subnodes
      inline bool HasSubNodes() const
      {
         return m_Nodes.size() != 0;
      }

      // Get all subnodes
      void GetSubNodes(NodeList& nodeList, NodeTypes nodeTypes);


   
   private:
      // Subnodes
      NodeMap m_Nodes;
   };
   
   // Constructor
   FileTree();

   // Destructor 
   ~FileTree();

   // Add a node
   Node* AddNode(const NodeName& path, NodeType type, UserData* userData = 0);

   // Get root node
   inline DirNode* GetRoot()
   {
      return &m_rootNode;
   }

   // Calculate node ids
   void CalcNodeIds();

   // Sort helper
   inline static bool SortNodesById(Node* node1, Node* node2)
   {
      return node1->GetId() < node2->GetId();
   }

   // Sort helper
   inline static bool SortNodesByIdReverse(Node* node1, Node* node2)
   {
      return node1->GetId() > node2->GetId();
   }



private:
   // The root node
   DirNode m_rootNode;

   // Calculate a node id
   void CalcNodeId(Node* node, int& currentId);
};





#endif /* FILE_TREE_H */
