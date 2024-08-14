
'		                Subjective Pronoun	Objective Pronoun	Reflective Pronoun	Possesive Pronoun
'
'		First Person	I (am)				Me			        Myself			    Mine
'		Second Person	You (are)			You			        Yourself		    Yours
'		Third Person	He/She (is)			Him/Her			    Himself/Herself		His/Hers
'				        It (is)				It			        Itself			    Its
'				        We (are)			Us			        Ourselves			Our
'				        They (are)			Them			    Themselves		    Theirs


Friend Enum PronounEnum As Integer
    None = -1
    Subjective = 0          ' I/You/He/She/It/We/They
    Objective = 1           ' Me/You/Him/Her/It/Us/Them
    Reflective = 2          ' Myself/Yourself/Himself/Herself/Itself/Ourselves/Themselves
    Possessive = 3          ' Mine/Yours/His/Hers/Its/Ours/Theirs
End Enum

Friend Class PronounInfo
    Public Key As String ' What is the pronoun applying to?
    Public Pronoun As PronounEnum
    Public Offset As Integer ' Where in the command does this substitution take place
    Public Gender As clsCharacter.GenderEnum
End Class

Friend Class PronounInfoList
    Inherits List(Of PronounInfo)

    Shadows Sub Add(ByVal sKey As String, ByVal ePronoun As PronounEnum, ByVal Gender As clsCharacter.GenderEnum, ByVal iOffset As Integer)
        Dim pi As New PronounInfo
        pi.Key = sKey
        pi.Pronoun = ePronoun
        pi.Offset = iOffset
        pi.Gender = Gender
        MyBase.Add(pi)

        ' Ensure the list is sorted by offset, for checking previous pronouns
        MyBase.Sort(Function(x, y) x.Offset.CompareTo(y.Offset))
    End Sub

End Class
