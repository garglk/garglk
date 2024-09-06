Public Class clsUserFunction
    Inherits clsItem

    Public Property Name As String
    Friend Property Output As Description

    Friend Enum ArgumentType
        [Object]
        Character
        Location
        Number
        Text
    End Enum

    Friend Class Argument
        Public Property Name As String
        Public Property Type As ArgumentType
    End Class
    Friend Arguments As List(Of Argument)

    Public Sub New()
        Output = New Description
        Arguments = New List(Of Argument)
    End Sub

    Friend Overrides ReadOnly Property AllDescriptions As System.Collections.Generic.List(Of SharedModule.Description)
        Get
            Dim all As New List(Of Description)
            all.Add(Output)
            Return all
        End Get
    End Property

    Public Overrides ReadOnly Property Clone As clsItem
        Get
            Return CType(Me.MemberwiseClone, clsItem)
        End Get
    End Property

    Public Overrides ReadOnly Property CommonName As String
        Get
            Return Name
        End Get
    End Property

    Public Overrides Function DeleteKey(sKey As String) As Boolean

        For Each d As Description In AllDescriptions
            If Not d.DeleteKey(sKey) Then Return False
        Next

        Return True
    End Function

    Friend Overrides Function FindStringLocal(sSearchString As String, Optional sReplace As String = Nothing, Optional bFindAll As Boolean = True, Optional ByRef iReplacements As Integer = 0) As Object
        Dim iCount As Integer = iReplacements
        iReplacements += MyBase.FindStringInStringProperty(Me.Name, sSearchString, sReplace, bFindAll)
        Return iReplacements - iCount
    End Function

    Public Overrides Function ReferencesKey(sKey As String) As Integer

    End Function
End Class
