Public Class clsMacro
    Implements ICloneable

    Public Sub New(ByVal sKey As String)
        MyBase.New()
        Key = sKey
    End Sub

    Private sTitle As String
    Public Property Title() As String
        Get
            Return sTitle
        End Get
        Set(ByVal value As String)
            ' TODO: Ensure unique for this adventure
            sTitle = value
        End Set
    End Property

    Private sCommands As String
    Public Property Commands() As String
        Get
            Return sCommands
        End Get
        Set(ByVal value As String)
            sCommands = value
        End Set
    End Property

    Private bShared As Boolean
    Public Property [Shared]() As Boolean
        Get
            Return bShared
        End Get
        Set(ByVal value As Boolean)
            bShared = value
        End Set
    End Property

    Private sIFID As String
    Public Property IFID() As String
        Get
            Return sIFID
        End Get
        Set(ByVal value As String)
            sIFID = value
        End Set
    End Property
    Public Function Clone() As Object Implements System.ICloneable.Clone
        Return Me.MemberwiseClone
    End Function

    Private sKey As String
    Public Property Key() As String
        Get
            Return sKey
        End Get
        Set(ByVal value As String)
            sKey = value
        End Set
    End Property

End Class