/*
 *   Copyright 2003, 2006 Michael J. Roberts
 *   
 *   Path Finder.  This module provides a class that implements Dijkstra's
 *   Algorithm for finding the shortest path between two nodes in a graph.
 *   The basic path finder class makes no assumptions about the nature of
 *   the underlying graph; subclasses must provide the concrete details
 *   about the graph being traversed.  We provide a subclass that finds
 *   paths in a game-world location map; this can be used for things like
 *   making an NPC find its own way to a destination.
 *   
 *   Everyone is granted permission to use and modify this file for any
 *   purpose, provided that the original author is credited.  
 */

#include <tads.h>


/* ------------------------------------------------------------------------ */
/*
 *   The basic path finder class.  This implements Dijkstra's algorithm to
 *   find the shortest path from one node to another in a graph.  This base
 *   implementation doesn't make any assumptions about the nature of the
 *   underlying graph; each subclass must override one method to provide
 *   the concrete graph implementation.  
 */
class BasePathFinder: object
    /* 
     *   Find the best path from 'fromNode' to 'toNode', which are nodes in
     *   the underlying graph.  We'll return a vector consisting of graph
     *   nodes, in order, giving the shortest path between the nodes.  Note
     *   that 'fromNode' and 'toNode' are included in the returned vector.
     *   
     *   If the two nodes 'fromNode' and 'toNode' aren't connected in the
     *   graph, we'll simply return nil.  
     */
    findPath(fromNode, toNode)
    {
        local i;
        local len;
        local cur;
        local workingList;
        local doneList;
        local toEntry;
        local ret;
        
        /* start with set containing the initial node */
        workingList = new Vector(32);
        workingList.append(new PathFinderNode(fromNode));

        /*
         *   Work through the list.  For each item in the list, add all of
         *   the items adjacent to that item to the end of the list.  Keep
         *   visiting new elements until we've visited everything in the
         *   list once.
         *   
         *   We'll only add an element to the list if it's not already in
         *   the list.  This guarantees that the loop will converge (since
         *   the number of items in the graph must be finite).  
         */
        for (i = 1 ; i <= workingList.length() ; ++i)
        {
            /* add each adjacent item to the working list */
            forEachAdjacent(workingList[i].graphNode,
                            new function(adj, dist)
            {
                /* 
                 *   add the adjacent node only if it's not already in the
                 *   working list 
                 */
                if (workingList.indexWhich({x: x.graphNode == adj}) == nil)
                    workingList.append(new PathFinderNode(adj));
            });
        }

        /* 
         *   if the destination isn't in the working list, then there's no
         *   hope of finding a path to it 
         */
        if (workingList.indexWhich({x: x.graphNode == toNode}) == nil)
            return nil;

        /* start with an empty "done" list */
        doneList = new Vector(32);

        /* we know the distance from the starting element to itself is zero */
        cur = workingList[1];
        cur.bestDist = 0;
        cur.predNode = nil;

        /* keep going while we have unresolved nodes */
        while (workingList.length() != 0)
        {
            local minIdx;
            local minDist;
            
            /* find the working list element with the shortest distance */
            minDist = nil;
            minIdx = nil;
            for (i = 1, len = workingList.length() ; i <= len ; ++i)
            {
                /* if this is the best so far, remember it */
                cur = workingList[i];
                if (cur.bestDist != nil
                    && (minDist == nil || cur.bestDist < minDist))
                {
                    /* this is the best so far */
                    minDist = cur.bestDist;
                    minIdx = i;
                }
            }

            /* move the best one to the 'done' list */
            cur = workingList[minIdx];
            doneList.append(cur = workingList[minIdx]);
            workingList.removeElementAt(minIdx);

            /* 
             *   update the best distance for everything adjacent to the
             *   one we just finished 
             */
            forEachAdjacent(cur.graphNode, new function(adj, dist)
            {
                local newDist;
                local entry;
                
                /* 
                 *   Find the working list entry from the adjacent room.
                 *   If there's no working list entry, there's nothing we
                 *   need to do here, since we must already be finished
                 *   with it.  
                 */
                entry = workingList.valWhich({x: x.graphNode == adj});
                if (entry == nil)
                    return;
                
                /* 
                 *   calculate the new distance to the adjacent room, if
                 *   we were to take a path from the room we just finished
                 *   - this is simply the path distance to the
                 *   just-finished room plus the distance from that room
                 *   to the adjacent room (i.e., 'dist') 
                 */
                newDist = cur.bestDist + dist;

                /* 
                 *   If this is better than the best estimate for the
                 *   adjacent room so far, assume we'll use this path.
                 *   Note that if the best estimate so far is nil, it
                 *   means we haven't found any path to the adjacent node
                 *   yet, so this new distance is definitely the best so
                 *   far.  
                 */
                if (entry.bestDist == nil
                    || newDist < entry.bestDist)
                {
                    /* it's the best so far - remember it */
                    entry.bestDist = newDist;
                    entry.predNode = cur;
                }
            });
        }
    
        /*
         *   We've exhausted the working list, so we know the best path to
         *   every node.  Now all that's left is to generate the list of
         *   nodes that takes us from here to there.
         *   
         *   The information we have in the 'done' list is in reverse
         *   order, because it tells us the predecessor for each node.
         *   So, first find out how long the path is by traversing the
         *   predecessor list from the ending point to the starting point.
         *   Note that the predecessor of the starting element is nil, so
         *   we can simply keep going until we reach a nil predecessor.  
         */
        toEntry = doneList.valWhich({x: x.graphNode == toNode});
        for (cur = toEntry, len = 0 ; cur != nil ;
             cur = cur.predNode, ++len) ;

        /* create the vector that represents the path */
        ret = new Vector(len);

        /* 
         *   Traverse the predecessor list again, filling in the vector.
         *   Since the predecessor list gives us the path in the reverse
         *   of the order we want, fill in the vector from the last
         *   element backwards, so that the vector ends up in the order we
         *   want.  In the return vector, store the nodes from the
         *   underlying graph (rather than our internal tracking entries).
         */
        for (cur = toEntry, i = len ; cur != nil ; cur = cur.predNode, --i)
            ret[i] = cur.graphNode;

        /* that's it - return the path */
        return ret;
    }

    /* 
     *   Iterate over everything adjacent to the given node in the
     *   underlying graph.  This routine must simply invoke the given
     *   callback function once for each graph node adjacent to the given
     *   graph node.
     *   
     *   The callback takes two arguments: the adjacent node, and the
     *   distance from 'node' to the adjacent node.  Note that the distance
     *   must be a positive number, as Dijkstra's algorithm depends on
     *   positive distances.  If the graph isn't weighted by distance,
     *   simply use 1 for all distances.
     *   
     *   This method isn't implemented in the base class, since we don't
     *   make any assumptions about the underlying graph.  Subclasses must
     *   provide concrete implementations of this routine to define the
     *   underlying graph.  
     */
    forEachAdjacent(node, func) { /* subclasses must override */ }
;

/* 
 *   A node entry for the path finder - this encapsulates the node in the
 *   underlying graph, along with the "label" information in the algorithm.
 *   Note that this is NOT a node in the underlying graph; rather, this is
 *   an internal data structure that we use in the path finder to keep
 *   track of the underlying nodes and their status in the work queue.  
 */
class PathFinderNode: object
    construct(node)
    {
        /* remember the underlying node */
        graphNode = node;
    }
    
    /* the underlying node in the graph */
    graphNode = nil

    /* 
     *   The best estimate of the shortest distance from the starting
     *   point.  We use nil to represent infinity here.
     */
    bestDist = nil

    /* the best-path predecessor for this path element */
    predNode = nil
;

/*
 *   Room path finder.  This is a concrete implementation of the path
 *   finder that finds a path from one Room to another in the game-world
 *   map.
 *   
 *   This implementation traverses rooms based on the actual connections in
 *   the game map.  Note that this isn't appropriate for all purposes,
 *   since it doesn't take into account what the actor knows about the game
 *   map.  This "omniscient" implementation is suitable for situations
 *   where the actor's knowledge isn't relevant and we just want the actual
 *   best path between the locations.  
 */
roomPathFinder: BasePathFinder
    /* find the path for a given actor from one room to another */
    findPath(actor, fromLoc, toLoc)
    {
        /* remember the actor */
        actor_ = actor;

        /* run the normal algorithm */
        return inherited(fromLoc, toLoc);
    }

    /* 
     *   iterate over the nodes adjacent in the underlying graph to the
     *   given node 
     */
    forEachAdjacent(loc, func)
    {
        /* 
         *   run through the directions, and add the apparent destination
         *   for each one 
         */
        foreach (local dir in Direction.allDirections)
        {
            local conn;
            local dest;
            
            /* 
             *   if there's a connector, and it has an apparent
             *   destination, then the apparent destination is the
             *   adjacent node 
             */
            if ((conn = loc.getTravelConnector(dir, actor_)) != nil
                && (dest = getDestination(loc, dir, conn)) != nil
                && includeRoom(dest))
            {
                /* 
                 *   This one seems to go somewhere - process the
                 *   destination.  The standard room map has no concept of
                 *   distance, so use equal weightings of 1 for all
                 *   inter-room distances.  
                 */
                (func)(dest, 1);
            }
        }
    }

    /*
     *   Get the location adjacent to the given location, for the purposes
     *   of finding the path.  By default, we return the actual
     *   destination, but subclasses might want to use other algorithms.
     *   For example, if a subclass's goal is to make an NPC find its own
     *   way from one location to another, then it should use the APPARENT
     *   destination, from the NPC's perspective, rather than the actual
     *   destination, since we'd want to construct the path based on the
     *   NPC's knowledge of the map.  
     */
    getDestination(loc, dir, conn)
    {
        /* return the actual destination for the connector */
        return conn.getDestination(loc, actor_);
    }

    /*
     *   For easier customization, this method allows the map that we
     *   search to be filtered so that it only includes a particular
     *   subset of the map.  This returns true if a given room is to be
     *   included in the search, nil if not.  By default, we include all
     *   rooms.  Note that this is only called to begin with for rooms
     *   that have apparent connections to the starting room, so there's
     *   no need to filter out areas of the map that aren't connected at
     *   all to the starting search location.  
     */
    includeRoom(loc) { return true; }

    /* the actor who's finding the path */
    actor_ = nil
;

/*
 *   An NPC goal-seeking path finder.  This is a subclass of the basic room
 *   path finder that 
 */
npcRoomPathFinder: roomPathFinder
    /* 
     *   Get the destination.  Unlike the base class implementation, we
     *   take into the NPC's knowledge of the map by only using connector
     *   destinations that are APPARENT to the NPC. 
     */
    getDestination(loc, dir, conn)
    {
        /* return the apparent destination of the connector */
        return conn.getApparentDestination(loc, actor_);
    }
;

