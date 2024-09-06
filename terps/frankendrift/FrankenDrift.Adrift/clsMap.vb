Imports Point = FrankenDrift.Glue.Point2D
Imports DashStyle = FrankenDrift.Glue.ConceptualDashStyle
Imports DashStyles = FrankenDrift.Glue.ConceptualDashStyle

Public Class Point3D
    Public X As Integer
    Public Y As Integer
    Public Z As Integer

    Public Sub New(ByVal X As Integer, ByVal Y As Integer, ByVal Z As Integer)
        Me.X = X
        Me.Y = Y
        Me.Z = Z
    End Sub
    Public Sub New()
    End Sub

    Public Shadows ReadOnly Property ToString() As String
        Get
            Return String.Format("X:{0}, Y:{1}, Z:{2}", X, Y, Z)
        End Get
    End Property
End Class



Public Class MapNode
    Implements ICloneable
    Implements IComparable(Of MapNode)

    Public Key As String
    Public Text As String
    Public Location As New Point3D
    Public Height As Integer = 4
    Public Width As Integer = 6
    Public Page As Integer
    Public Pinned As Boolean = False
    Public Anchors As New Dictionary(Of DirectionsEnum, Anchor)
    Public Links As New Generic.Dictionary(Of DirectionsEnum, MapLink)
    Private bOverlapping As Boolean = False
    Private bSeen As Boolean = False
    Public eInEdge, eOutEdge As DirectionsEnum
    Public ptIn, ptOut As Point
    Public bHasUp, bHasDown, bHasIn, bHasOut As Boolean
    Public bDrawIn, bDrawOut As Boolean

    Public Property Overlapping() As Boolean
        Get
            Return bOverlapping
        End Get
        Set(ByVal value As Boolean)
            If value <> bOverlapping Then
                bOverlapping = value
                If Adventure.Map IsNot Nothing Then
                    For Each p As MapPage In Adventure.Map.Pages.Values
                        For Each n As MapNode In p.Nodes
                            If n.Overlapping Then
                                Adventure.Map.FirstOverlapPage = p.iKey
                                Exit Property
                            End If
                        Next
                    Next
                End If
                Adventure.Map.FirstOverlapPage = -1
            End If
        End Set
    End Property


    Public Property Seen() As Boolean
        Get
            Return bSeen
        End Get
        Set(ByVal value As Boolean)
            If value <> bSeen Then
                bSeen = value
                If bSeen Then
                    Adventure.Map.Pages(Page).Seen = True
                Else
                    Adventure.Map.Pages(Page).RecalculateSeen()
                End If
            End If
        End Set
    End Property

    ' The translated points
    Public Points() As Point = {New Point(0, 0), New Point(0, 0), New Point(0, 0), New Point(0, 0)}
    Public ptUp As Point
    Public ptDown As Point

    Private Function Clone() As Object Implements System.ICloneable.Clone
        Return Me.MemberwiseClone
    End Function
    Public Function CloneMe() As MapNode
        Return CType(Clone(), MapNode)
    End Function

    Public Function CompareTo(ByVal other As MapNode) As Integer Implements System.IComparable(Of MapNode).CompareTo
        'Return other.Z.CompareTo(Me.Z)
        If Location.Z <> other.Location.Z Then Return other.Location.Z.CompareTo(Location.Z)
        If Location.Y <> other.Location.Y Then Return Location.Y.CompareTo(other.Location.Y) ' other.Y.CompareTo(Me.Y)
        Return Location.X.CompareTo(other.Location.X)
    End Function

    Public Sub New(Optional ByVal bCreateAnchors As Boolean = True)
        If bCreateAnchors Then
            For Each d As DirectionsEnum In New DirectionsEnum() {DirectionsEnum.NorthWest, DirectionsEnum.North, DirectionsEnum.NorthEast, DirectionsEnum.East, DirectionsEnum.SouthEast, DirectionsEnum.South, DirectionsEnum.SouthWest, DirectionsEnum.West}
                Dim anchor As New Anchor
                anchor.Direction = d
                anchor.Parent = Me
                Anchors.Add(d, anchor)
            Next
        End If
    End Sub

End Class



Public Class MapLink
    Public Style As DashStyle
    Public Duplex As Boolean
    Public sSource As String
    Public eSourceLinkPoint As DirectionsEnum
    Public sDestination As String
    Public eDestinationLinkPoint As DirectionsEnum
    Public OrigMidPoints() As Point3D = {} ' In case user wishes to enhance link
    Public Points() As Point
    Public ptStartB As Point
    Public ptEndB As Point
    Friend Anchors As New Generic.List(Of Anchor)
End Class


Public Class Anchor
    Public Points() As Point = {New Point(0, 0), New Point(0, 0), New Point(0, 0), New Point(0, 0)}
    Friend Direction As DirectionsEnum
    Public Parent As Object
    Public bVisible As Boolean

    Public Property Visible() As Boolean
        Get
            Return bVisible
        End Get
        Set(ByVal value As Boolean)
            If value <> bVisible Then
                bVisible = value
            End If
        End Set
    End Property
    Public HasLink As Boolean = False

    Public ReadOnly Property GetApproxPoint3D() As Point3D
        Get
            If TypeOf Parent Is MapNode Then
                Dim nodParent As MapNode = CType(Parent, MapNode)
                With nodParent.Location
                    Select Case Direction
                        Case DirectionsEnum.NorthWest
                            Return nodParent.Location
                        Case DirectionsEnum.North
                            Return New Point3D(.X + CInt(nodParent.Width / 2), .Y, .Z)
                        Case DirectionsEnum.NorthEast
                            Return New Point3D(.X + nodParent.Width, .Y, .Z)
                        Case DirectionsEnum.East
                            Return New Point3D(.X + nodParent.Width, .Y + CInt(nodParent.Height / 2), .Z)
                        Case DirectionsEnum.SouthEast
                            Return New Point3D(.X + nodParent.Width, .Y + nodParent.Height, .Z)
                        Case DirectionsEnum.South
                            Return New Point3D(.X + CInt(nodParent.Width / 2), .Y + nodParent.Height, .Z)
                        Case DirectionsEnum.SouthWest
                            Return New Point3D(.X, .Y + nodParent.Height, .Z)
                        Case DirectionsEnum.West
                            Return New Point3D(.X, .Y + CInt(nodParent.Height / 2), .Z)
                        Case Else
                            Return New Point3D(0, 0, 0)
                    End Select
                End With
            Else
                Return New Point3D(0, 0, 0)
            End If
        End Get
    End Property

End Class


Public Class MapPage

    Public iKey As Integer

    Private bSeen As Boolean
    Public Property Seen() As Boolean
        Get
            Return bSeen
        End Get
        Set(ByVal value As Boolean)
            If value <> bSeen Then
                bSeen = value
            End If
        End Set
    End Property


    Friend Sub RecalculateSeen()
        Dim lbSeen As Boolean = False
        For Each n As MapNode In Nodes
            If n.Seen Then
                lbSeen = True
                Exit For
            End If
        Next
        Seen = lbSeen
    End Sub


    Public Sub New(ByVal iKey As Integer)
        Me.iKey = iKey
        Label = "Page " & iKey + 1
    End Sub


    Public Label As String
    Public Nodes As New Generic.List(Of MapNode)


    Public Function GetNode(ByVal sKey As String) As MapNode
        For Each node As MapNode In Nodes
            If node.Key = sKey Then Return node
        Next
        Return Nothing
    End Function
    Public Sub AddNode(ByVal node As MapNode)
        Nodes.Add(node)
    End Sub
    Public Sub RemoveNode(ByVal node As MapNode)
        Nodes.Remove(node)
    End Sub


    Public Sub SortNodes()
        Nodes.Sort()
    End Sub


    ''' <summary>
    ''' Checks to see if any nodes overlap
    ''' </summary>
    ''' <returns>True, if any overlap</returns>
    ''' <remarks></remarks>
    Public Function CheckForOverlaps() As Boolean

        For Each node As MapNode In Nodes
            node.Overlapping = False
        Next

        For i As Integer = 0 To Nodes.Count - 1
            For j As Integer = i + 1 To Nodes.Count - 1
                If Nodes(i).Location.Z = Nodes(j).Location.Z Then
                    Dim AX1 As Integer = Nodes(i).Location.X
                    Dim AX2 As Integer = Nodes(i).Location.X + Nodes(i).Width
                    Dim AY1 As Integer = Nodes(i).Location.Y
                    Dim AY2 As Integer = Nodes(i).Location.Y + Nodes(i).Height
                    Dim BX1 As Integer = Nodes(j).Location.X
                    Dim BX2 As Integer = Nodes(j).Location.X + Nodes(j).Width
                    Dim BY1 As Integer = Nodes(j).Location.Y
                    Dim BY2 As Integer = Nodes(j).Location.Y + Nodes(j).Height

                    If AX1 < BX2 AndAlso AX2 > BX1 AndAlso AY1 < BY2 AndAlso AY2 > BY1 Then
                        Nodes(i).Overlapping = True
                        Nodes(j).Overlapping = True
                        CheckForOverlaps = True
                    End If
                End If
            Next
        Next

    End Function

End Class


Public Class clsMap

    Public Pages As New Generic.Dictionary(Of Integer, MapPage)
    Public SelectedPage As String
    Private iFirstOverlapPage As Integer = -1

    Public Property FirstOverlapPage() As Integer
        Get
            Return iFirstOverlapPage
        End Get
        Set(ByVal value As Integer)
            If iFirstOverlapPage <> value Then
            End If
        End Set
    End Property

    Public Sub RecalculateLayout()
        Pages.Clear()

        For Each sLocKey As String In Adventure.htblLocations.Keys
            AddNode(sLocKey)
        Next

        For Each page As MapPage In Pages.Values
            page.SortNodes()
        Next

        CheckForOverlaps()
        If Pages.Count = 0 Then Pages.Add(0, New MapPage(0))
    End Sub


    Public Sub CheckForOverlaps()
        For Each page As MapPage In Pages.Values
            page.CheckForOverlaps()
        Next
    End Sub


    Public Sub DeleteNode(ByVal sKey As String)
        Dim node As MapNode = FindNode(sKey)
        If node IsNot Nothing Then
            Pages(node.Page).RemoveNode(node)
        End If
    End Sub


    Public Sub RefreshNode(ByVal sKey As String)
        If Not Adventure.htblLocations.ContainsKey(sKey) Then Exit Sub

        Dim loc As clsLocation = Adventure.htblLocations(sKey)
        Dim node As MapNode = FindNode(sKey)

        If node Is Nothing Then Exit Sub

        ' Add any links out to other locations
        For d As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
            If loc.arlDirections(d).LocationKey IsNot Nothing Then
                Select Case d
                    Case DirectionsEnum.Up
                        node.bHasUp = True
                    Case DirectionsEnum.Down
                        node.bHasDown = True
                    Case DirectionsEnum.In
                        node.bHasIn = True
                    Case DirectionsEnum.Out
                        node.bHasOut = True
                End Select
            End If
        Next
    End Sub


    Public Sub UpdateMap(ByVal loc As clsLocation)
        Dim node As MapNode = FindNode(loc.Key)

        If node Is Nothing Then
            node = AddNode(loc.Key)
        Else
            node.Text = loc.ShortDescriptionSafe
            For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
                If loc.arlDirections(d).LocationKey <> "" Then
                    Dim nodDest As MapNode = FindNode(loc.arlDirections(d).LocationKey)
                    If nodDest IsNot Nothing Then
                        If node.Page <> nodDest.Page AndAlso d <> DirectionsEnum.In AndAlso d <> DirectionsEnum.Out Then
                            ' Display arrows, since the location is on a different page
                            If Not node.Links.ContainsKey(d) Then
                                Dim l As New MapLink
                                node.Links.Add(d, l)
                            End If
                            With node.Links(d)
                                .sSource = node.Key
                                .sDestination = node.Key
                                .eSourceLinkPoint = d
                                .eDestinationLinkPoint = OppositeDirection(d)
                                .Duplex = False
                            End With

                            ' Assuming the location on the other page actually points back at us, update it too
                            Dim locDest As clsLocation = Adventure.htblLocations(loc.arlDirections(d).LocationKey)
                            If locDest.arlDirections(OppositeDirection(d)).LocationKey = node.Key Then
                                If Not nodDest.Links.ContainsKey(OppositeDirection(d)) OrElse nodDest.Links(OppositeDirection(d)).sDestination <> nodDest.Key Then
                                    ' Update the destination node to make sure that it points back here
                                    UpdateMap(locDest)
                                End If
                            End If

                        Else
                            If node.Links.ContainsKey(d) Then
                                With node.Links(d)
                                    If .sDestination = loc.arlDirections(d).LocationKey Then
                                        ' Nothing to do
                                    Else
                                        ' If the old destination still points here, create a non-duplex link
                                        If .Duplex AndAlso Adventure.htblLocations(.sDestination).arlDirections(.eDestinationLinkPoint).LocationKey = node.Key Then
                                            Dim nodeOldDest As MapNode = FindNode(.sDestination)
                                            Dim l As New MapLink
                                            l.sSource = nodeOldDest.Key
                                            l.sDestination = node.Key
                                            l.eSourceLinkPoint = .eDestinationLinkPoint
                                            l.eDestinationLinkPoint = .eSourceLinkPoint
                                            l.Duplex = False
                                            nodeOldDest.Links.Add(l.eSourceLinkPoint, l)
                                        End If
                                        ' If the destination points at itself, then delete that link and make us a Duplex (we may have just moved back to it's page)
                                        If nodDest.Links.ContainsKey(OppositeDirection(d)) AndAlso nodDest.Links(OppositeDirection(d)).sDestination = nodDest.Key Then
                                            nodDest.Links.Remove(OppositeDirection(d))
                                            node.Links(d).Duplex = True
                                        End If
                                        ' Then create our new link
                                        .sDestination = loc.arlDirections(d).LocationKey
                                        If Adventure.htblLocations(.sDestination).arlDirections(.eDestinationLinkPoint).LocationKey = node.Key Then .Duplex = True
                                    End If
                                    If .eSourceLinkPoint = DirectionsEnum.In Then node.bHasIn = True
                                    If .eSourceLinkPoint = DirectionsEnum.Out Then node.bHasOut = True
                                    If .eDestinationLinkPoint = DirectionsEnum.In Then nodDest.bHasIn = True
                                    If .eDestinationLinkPoint = DirectionsEnum.Out Then nodDest.bHasOut = True
                                End With
                            Else
                                ' See if the destination node already links to us, non-duplex
                                Dim bFound As Boolean = False
                                For Each lDest As MapLink In nodDest.Links.Values
                                    If lDest.sDestination = node.Key AndAlso lDest.eDestinationLinkPoint = d Then
                                        bFound = True
                                        lDest.Duplex = True
                                    End If
                                Next
                                If Not bFound Then
                                    Dim l As New MapLink
                                    l.sSource = node.Key
                                    l.sDestination = loc.arlDirections(d).LocationKey
                                    l.eSourceLinkPoint = d
                                    l.eDestinationLinkPoint = OppositeDirection(d)
                                    If Adventure.htblLocations(l.sDestination).arlDirections(l.eDestinationLinkPoint).LocationKey = node.Key Then l.Duplex = True
                                    node.Links.Add(d, l)
                                    If l.eSourceLinkPoint = DirectionsEnum.In Then node.bHasIn = True
                                    If l.eSourceLinkPoint = DirectionsEnum.Out Then node.bHasOut = True
                                    If l.eDestinationLinkPoint = DirectionsEnum.In Then nodDest.bHasIn = True
                                    If l.eDestinationLinkPoint = DirectionsEnum.Out Then nodDest.bHasOut = True
                                End If
                            End If
                        End If
                    End If
                Else
                    ' See if we've removed a link
                    If node.Links.ContainsKey(d) Then
                        ' Ok, need to remove the link, or make it a non-duplex link from other end
                        With node.Links(d)
                            If .Duplex Then
                                ' Ok, create new non-duplex link
                                Dim nodeOldDest As MapNode = FindNode(.sDestination)
                                Dim l As New MapLink
                                l.sSource = nodeOldDest.Key
                                l.sDestination = node.Key
                                l.eSourceLinkPoint = .eDestinationLinkPoint
                                l.eDestinationLinkPoint = .eSourceLinkPoint
                                l.Duplex = False
                                nodeOldDest.Links.Add(l.eSourceLinkPoint, l)
                            End If
                        End With
                        node.Links.Remove(d)
                    End If
                    ' Need to see if any node has a link that terminates here
                    For Each n As MapNode In Pages(node.Page).Nodes
                        For Each l As MapLink In n.Links.Values
                            If l.sDestination = node.Key AndAlso l.eDestinationLinkPoint = d Then
                                l.Duplex = False
                            End If
                        Next
                    Next
                End If
            Next
        End If
    End Sub


    Private Function GetNewLocation(ByVal node As MapNode, ByVal nodeFrom As MapNode, ByVal dirFrom As DirectionsEnum) As Point3D
        Dim ptLocation As New Point3D
        ptLocation.X = nodeFrom.Location.X
        ptLocation.Y = nodeFrom.Location.Y
        ptLocation.Z = nodeFrom.Location.Z

        Select Case dirFrom
            Case DirectionsEnum.North
                ptLocation.Y = nodeFrom.Location.Y + nodeFrom.Height + 2
            Case DirectionsEnum.NorthEast
                ptLocation.X = nodeFrom.Location.X - node.Width - 3
                ptLocation.Y = nodeFrom.Location.Y + nodeFrom.Height + 2
            Case DirectionsEnum.East
                ptLocation.X = nodeFrom.Location.X - node.Width - 3
            Case DirectionsEnum.SouthEast
                ptLocation.X = nodeFrom.Location.X - node.Width - 3
                ptLocation.Y = nodeFrom.Location.Y - node.Height - 2
            Case DirectionsEnum.South
                ptLocation.Y = nodeFrom.Location.Y - node.Height - 2
            Case DirectionsEnum.SouthWest
                ptLocation.X = nodeFrom.Location.X + nodeFrom.Width + 3
                ptLocation.Y = nodeFrom.Location.Y - node.Height - 2
            Case DirectionsEnum.West
                ptLocation.X = nodeFrom.Location.X + nodeFrom.Width + 3
            Case DirectionsEnum.NorthWest
                ptLocation.X = nodeFrom.Location.X + nodeFrom.Width + 3
                ptLocation.Y = nodeFrom.Location.Y + nodeFrom.Height + 2
            Case DirectionsEnum.Up
                ptLocation.Z += 6
            Case DirectionsEnum.Down
                ptLocation.Z -= 6
        End Select

        Return ptLocation
    End Function


    Private Function AddNode(ByVal sLocKey As String, Optional ByVal nodeFrom As MapNode = Nothing, Optional ByVal dirFrom As DirectionsEnum = Nothing) As MapNode
        Dim loc As clsLocation = Adventure.htblLocations(sLocKey)
        Dim node As MapNode = FindNode(loc.Key)

        If node Is Nothing Then
            node = New MapNode

            node.Key = sLocKey
            node.Text = loc.ShortDescriptionSafe
            If nodeFrom Is Nothing Then
                node.Page = GetNewPage()
                node.Location.X = 0
                node.Location.Y = 0
                node.Location.Z = 0
            Else
                node.Page = nodeFrom.Page
                node.Location = GetNewLocation(node, nodeFrom, dirFrom)
                If dirFrom = DirectionsEnum.In OrElse dirFrom = DirectionsEnum.Out Then node.Page = GetNewPage()
            End If

            If Not Pages.ContainsKey(node.Page) Then Pages.Add(node.Page, New MapPage(node.Page))
            Pages(node.Page).AddNode(node)

            ' Add any links out to other locations
            For d As DirectionsEnum = DirectionsEnum.North To DirectionsEnum.NorthWest
                If loc.arlDirections(d).LocationKey IsNot Nothing AndAlso loc.arlDirections(d).LocationKey <> "" Then
                    Select Case d
                        Case DirectionsEnum.Up
                            node.bHasUp = True
                        Case DirectionsEnum.Down
                            node.bHasDown = True
                        Case DirectionsEnum.In
                            node.bHasIn = True
                        Case DirectionsEnum.Out
                            node.bHasOut = True
                    End Select
                    AddNode(loc.arlDirections(d).LocationKey, node, OppositeDirection(d))
                    ' First, check to see if the other location already has our link
                    Dim nodeDest As MapNode = Nothing
                    If d = DirectionsEnum.In OrElse d = DirectionsEnum.Out Then
                        nodeDest = FindNode(loc.arlDirections(d).LocationKey)
                    Else
                        nodeDest = Pages(node.Page).GetNode(loc.arlDirections(d).LocationKey)
                    End If

                    Dim link As MapLink = Nothing
                    If nodeDest IsNot Nothing Then
                        If nodeDest.Links.ContainsKey(OppositeDirection(d)) Then
                            link = nodeDest.Links(OppositeDirection(d))
                        Else
                            For Each linkDest As MapLink In nodeDest.Links.Values
                                If linkDest.sDestination = sLocKey Then
                                    ' Make assumption that this is our link - in some layouts this may not be true
                                    link = linkDest
                                    Exit For
                                End If
                            Next
                        End If
                    End If

                    If link Is Nothing Then
                        AddLink(node, loc.arlDirections(d).LocationKey, d, OppositeDirection(d))
                    Else
                        link.sDestination = sLocKey
                        link.eDestinationLinkPoint = d
                        If DottedLink(loc.arlDirections(d)) Then
                            link.Style = DashStyles.Dot
                        End If
                        link.Duplex = True
                        If Pages(node.Page).GetNode(sLocKey).Anchors.ContainsKey(d) Then Pages(node.Page).GetNode(sLocKey).Anchors(d).HasLink = True
                    End If
                End If
            Next
        Else
            ' Check to see if we need to merge pages
            If nodeFrom IsNot Nothing AndAlso node.Page <> nodeFrom.Page Then
                If dirFrom <> DirectionsEnum.In AndAlso dirFrom <> DirectionsEnum.Out Then
                    MergePages(node, nodeFrom)
                End If
            End If
        End If
        Return node
    End Function


    Friend Function DottedLink(dir As clsDirection) As Boolean
        If dir.Restrictions.Count > 0 Then
            Return True
        Else
            Return False
        End If
    End Function


    Private Sub AddLink(ByVal nodeSource As MapNode, ByVal sDest As String, ByVal eSourcePoint As DirectionsEnum, ByVal eDestPoint As DirectionsEnum)
        Dim link As New MapLink
        Dim loc As clsLocation = Adventure.htblLocations(nodeSource.Key)

        link.sSource = nodeSource.Key
        link.sDestination = sDest
        link.Duplex = False
        If DottedLink(loc.arlDirections(eSourcePoint)) Then
            link.Style = DashStyles.Dot
        Else
            link.Style = DashStyles.Solid
        End If
        link.eSourceLinkPoint = eSourcePoint
        ' Make assumption that end point is opposite of this one
        link.sDestination = sDest
        link.eDestinationLinkPoint = eDestPoint
        If Not nodeSource.Links.ContainsKey(eSourcePoint) Then nodeSource.Links.Add(eSourcePoint, link)
        If nodeSource.Anchors.ContainsKey(eSourcePoint) Then nodeSource.Anchors(eSourcePoint).HasLink = True
    End Sub


    Public Sub MergePages(ByVal node1 As MapNode, ByVal node2 As MapNode)
        Dim pageFrom As MapPage
        Dim pageTo As MapPage
        Dim nodeMoving As MapNode
        Dim nodeStaying As MapNode

        If Pages(node1.Page).Nodes.Count > Pages(node2.Page).Nodes.Count OrElse (Pages(node1.Page).Nodes.Count = Pages(node2.Page).Nodes.Count AndAlso node2.Page > node1.Page) Then
            nodeMoving = node2
            nodeStaying = node1
        Else
            nodeMoving = node1
            nodeStaying = node2
        End If
        pageFrom = Pages(nodeMoving.Page)
        pageTo = Pages(nodeStaying.Page)

        ' Work out what to offset each node by
        ' First, get correct location for 
        Dim locMoving As clsLocation = Adventure.htblLocations(nodeMoving.Key)
        Dim locStaying As clsLocation = Adventure.htblLocations(nodeStaying.Key)
        Dim dirMove As DirectionsEnum

        For Each d As DirectionsEnum In [Enum].GetValues(GetType(DirectionsEnum))
            If locMoving.arlDirections(d).LocationKey = locStaying.Key Then
                AddLink(nodeStaying, nodeMoving.Key, OppositeDirection(d), d)
                dirMove = d
            End If
        Next

        Dim ptNewLocation As Point3D = GetNewLocation(nodeMoving, nodeStaying, dirMove)
        Dim iXOffset As Integer = nodeMoving.Location.X - ptNewLocation.X
        Dim iYOffset As Integer = nodeMoving.Location.Y - ptNewLocation.Y
        Dim iZOffset As Integer = nodeMoving.Location.Z - ptNewLocation.Z

        For Each n As MapNode In pageFrom.Nodes
            pageTo.AddNode(n)
            n.Page = pageTo.iKey
            n.Location.X -= iXOffset
            n.Location.Y -= iYOffset
            n.Location.Z -= iZOffset
        Next
        pageFrom.Nodes.Clear()
        Pages.Remove(pageFrom.iKey)
    End Sub

    Public Function FindNode(ByVal sLocKey As String) As MapNode
        For Each page As MapPage In Pages.Values
            For Each node As MapNode In page.Nodes
                If node.Key = sLocKey Then Return node
            Next
        Next
        Return Nothing
    End Function

    Public Function GetNewPage(Optional ByVal bAllowEmptyPages As Boolean = False) As Integer
        Dim iPage As Integer = 0
        While Pages.ContainsKey(iPage) AndAlso (bAllowEmptyPages OrElse Pages(iPage).Nodes.Count > 0)
            iPage += 1
        End While
        Return iPage
    End Function

    Public Sub New()
        Pages.Add(0, New MapPage(0))
    End Sub

End Class