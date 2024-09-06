
Friend Class clsSingleItem
    Public Sub New()
        Me.MatchingPossibilities = New StringArrayList
    End Sub
    Public Sub New(ByVal sKey As String)
        Me.New()
        MatchingPossibilities.Add(sKey)
    End Sub

    Public MatchingPossibilities As StringArrayList
    Public bExplicitlyMentioned As Boolean
    Public sCommandReference As String
End Class

Friend Class ItemArrayList
    Inherits Generic.List(Of clsSingleItem)

    Shadows Sub Add(ByVal itm As clsSingleItem)
        MyBase.Add(itm)
    End Sub

    Shadows Function ContainsKey(ByVal sKey As String) As Boolean
        Return MyBase.Any(Function(i) i.MatchingPossibilities.Contains(sKey))
    End Function

End Class

Friend Class clsNewReference
    Implements ICloneable

    Public sParentTask As String

    Public Function ContainsKey(ByVal sKey As String) As Boolean
        Return Items.ContainsKey(sKey)
    End Function

    Public Sub New(ByRef ReferenceType As ReferencesType)
        Me.Items = New ItemArrayList
        Me.ReferenceType = ReferenceType
        Me.Index = -1
    End Sub

    Public Items As ItemArrayList ' clsSingleItem
    'Public Multiple As Boolean ' This is true if Items.Length > 1
    Public ReferenceType As ReferencesType
    Public Index As Integer
    Public ReferenceMatch As String ' i.e. "object2", "character3" etc.

    Public Function Clone() As Object Implements System.ICloneable.Clone
        Return Me.MemberwiseClone
    End Function
End Class
