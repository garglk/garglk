Imports FrankenDrift.Glue

Public Class clsBabelTreatyInfo

    Public Sub New()
        sVersion = "1.0"
        ReDim oStories(0)
        oStories(0) = New clsStory
    End Sub


    Public Function FromString(ByVal sXML As String) As Boolean

        Return True

    End Function



    Private oStories() As clsStory
    Public Property Stories() As clsStory()
        Get
            Return oStories
        End Get
        Set(ByVal Value As clsStory())
            oStories = Value
        End Set
    End Property


    Private sVersion As String
    Public Property Version() As String
        Get
            Return sVersion
        End Get
        Set(ByVal value As String)
            sVersion = value
        End Set
    End Property


    Public Class clsStory

        Public Sub New()
            Identification.GenerateNewIFID()
            Releases = New clsBabelTreatyInfo.clsStory.clsReleases
        End Sub


        ' The <identification> section is mandatory. The content here identifies
        ' to which story file the metadata belongs. (This is necessary because
        ' the metadata may be held on some remote server, quite separate from
        ' the story file.)
        Public Class clsIdentification

            Public Sub GenerateNewIFID()
                With System.Reflection.Assembly.GetExecutingAssembly.GetName.Version
                    sIFID = "ADRIFT-" & .Major & .Minor.ToString("00") & "-" & sRight(Guid.NewGuid.ToString, 32).ToUpper
                End With
            End Sub

            Private sIFID As String
            Public Property IFID() As String
                Get
                    Return sIFID
                End Get
                Set(ByVal value As String)
                    sIFID = value
                End Set
            End Property

            Public Property Format() As String
                Get
                    Return "adrift"
                End Get
                Set(ByVal value As String)
                    ' Ignore
                End Set
            End Property

        End Class
        Public Identification As New clsIdentification

        ' The <bibliographic> section is mandatory.
        Public Class clsBibliographic

            Public Property Title() As String
                Get
                    Return Adventure.Title
                End Get
                Set(ByVal value As String)
                    ' Ignore
                End Set
            End Property

            Public Property Author() As String
                Get
                    Return Adventure.Author
                End Get
                Set(ByVal value As String)
                    ' Ignore
                End Set
            End Property

            ' Always reads this from the current culture, so can change if modified on a different machine
            Private sLanguage As String
            Public Property Language() As String
                Get
                    Return Threading.Thread.CurrentThread.CurrentCulture.ToString()
                End Get
                Set(ByVal value As String)
                    ' Ignore
                End Set
            End Property

            Private sHeadline As String
            Public Property Headline() As String
                Get
                    Return sHeadline
                End Get
                Set(ByVal value As String)
                    sHeadline = value
                End Set
            End Property

            Private sDescription As String
            Public Property Description() As String
                Get
                    Return sDescription
                End Get
                Set(ByVal value As String)
                    sDescription = value
                End Set
            End Property

            Private sSeries As String
            Public Property Series() As String
                Get
                    Return sSeries
                End Get
                Set(ByVal value As String)
                    sSeries = value
                End Set
            End Property

            Private iSeriesNumber As Integer
            Public Property SeriesNumber() As Integer
                Get
                    Return iSeriesNumber
                End Get
                Set(ByVal value As Integer)
                    iSeriesNumber = value
                    If iSeriesNumber > 0 Then bSeriesNumberSpecified = True
                End Set
            End Property

            Private bSeriesNumberSpecified As Boolean
            Public Property SeriesNumberSpecified() As Boolean
                Get
                    Return bSeriesNumberSpecified
                End Get
                Set(ByVal Value As Boolean)
                    bSeriesNumberSpecified = Value
                End Set
            End Property

            Public Enum ForgivenessEnum
                Merciful
                Polite
                Tough
                Nasty
                Cruel
            End Enum
            Private eForgiveness As ForgivenessEnum
            Public Property Forgiveness() As ForgivenessEnum
                Get
                    Return eForgiveness
                End Get
                Set(ByVal value As ForgivenessEnum)
                    eForgiveness = value
                    bForgivenessSpecified = True
                End Set
            End Property

            Private bForgivenessSpecified As Boolean
            Public Property ForgivenessSpecified() As Boolean
                Get
                    Return bForgivenessSpecified
                End Get
                Set(ByVal Value As Boolean)
                    bForgivenessSpecified = Value
                End Set
            End Property

        End Class
        Public Bibliographic As New clsBibliographic

        ' The <resources> tag is optional. This section, if present, details
        ' the other files (if any) which are intended to accompany the story
        ' file, and to be available to any player. By "other" is meant files
        ' which are not embedded in the story file. (So, for instance, pictures
        ' in a blorbed Z-machine story file do not count as "other".)
        Public Class clsResources

            Public Class clsAuxiliary

                Private sLeafName As String
                Public Property LeafName() As String
                    Get
                        Return sLeafName
                    End Get
                    Set(ByVal value As String)
                        sLeafName = value
                    End Set
                End Property

                Private sDescription As String
                Public Property Description() As String
                    Get
                        Return sDescription
                    End Get
                    Set(ByVal value As String)
                        sDescription = value
                    End Set
                End Property

            End Class
            Public Auxiliary As New clsAuxiliary

        End Class
        Public Resources As clsResources

        ' The <contacts> tag is optional.
        Public Class clsContacts

            Private sURL As String
            Public Property URL() As String
                Get
                    Return sURL
                End Get
                Set(ByVal value As String)
                    sURL = value
                End Set
            End Property

            Private sAuthorEmail As String
            Public Property AuthorEmail() As String
                Get
                    Return sAuthorEmail
                End Get
                Set(ByVal value As String)
                    sAuthorEmail = value
                End Set
            End Property

        End Class
        Public Contacts As clsContacts

        ' The <cover> tag is optional, except that it is mandatory for an
        ' iFiction record embedded in a story file which contains a cover
        ' image; and the information must, of course, be correct.

        Public Class clsCover
            Friend imgCoverArt As Byte()


            ' This is required to be either "jpg" or "png". No other casings,
            ' spellings or image formats are permitted.
            Private sFormat As String
            Public Property Format() As String
                Get
                    If imgCoverArt IsNot Nothing Then Return sFormat Else Return Nothing
                End Get
                Set(ByVal value As String)
                    If value = "" OrElse value = "jpg" OrElse value = "png" Then
                        sFormat = value
                    Else
                        Throw New Exception("Only jpg or png allowed!")
                    End If
                End Set
            End Property

            Public Property Height() As Integer
                Get
                    Return 0
                End Get
                Set(ByVal value As Integer)
                    ' Provided for serialization only
                End Set
            End Property

            Private bHeightSpecified As Boolean
            Public Property HeightSpecified() As Boolean
                Get
                    Return False
                End Get
                Set(ByVal Value As Boolean)
                    ' Ignore
                End Set
            End Property

            Public Property Width() As Integer
                Get
                    Return 0
                End Get
                Set(ByVal value As Integer)
                    ' Provided for serialization only
                End Set
            End Property

            Private bWidthSpecified As Boolean
            Public Property WidthSpecified() As Boolean
                Get
                    Return False
                End Get
                Set(ByVal Value As Boolean)
                    ' Ignore
                End Set
            End Property

        End Class
        Public Cover As clsCover

        Public Class clsADRIFT

            Public ReadOnly Property Version() As String
                Get
                    Return Application.ProductVersion
                End Get
            End Property

        End Class
        Public ADRIFT As clsADRIFT

        ' 5.11 - Releases
        '   Attached
        '   History
        Public Class clsReleases

            Public Sub New()
                Attached = New clsBabelTreatyInfo.clsStory.clsReleases.clsAttached
            End Sub


            Public Class clsAttached

                Public Sub New()
                    Release = New clsBabelTreatyInfo.clsStory.clsReleases.clsAttached.clsRelease
                End Sub


                Public Class clsRelease

                    Private iVersion As Integer = 1
                    Public Property Version() As Integer
                        Get
                            Return iVersion
                        End Get
                        Set(ByVal value As Integer)
                            iVersion = value
                            If iVersion > 0 Then bVersionSpecified = True
                        End Set
                    End Property

                    Private bVersionSpecified As Boolean
                    Public Property VersionSpecified() As Boolean
                        Get
                            Return bVersionSpecified
                        End Get
                        Set(ByVal Value As Boolean)
                            bVersionSpecified = Value
                        End Set
                    End Property

                    Private dtReleaseDate As DateTime
                    Public Property ReleaseDateXML() As String
                        Get
                            Return ReleaseDate.ToString("yyyy-MM-dd", System.Globalization.CultureInfo.InvariantCulture.DateTimeFormat)
                        End Get
                        Set(ByVal value As String)
                            ReleaseDate = DateTime.Parse(value)
                        End Set
                    End Property

                    Public Property ReleaseDate() As DateTime
                        Get
#If Generator Then
                            Return Date.Today
#Else
                            Return dtReleaseDate
#End If
                        End Get
                        Set(ByVal value As DateTime)
                            dtReleaseDate = value
                        End Set
                    End Property

                    Private sCompiler As String
                    Public Property Compiler() As String
                        Get
                            Return "ADRIFT 5"
                        End Get
                        Set(ByVal value As String)
                        End Set
                    End Property

                    Private sCompilerVersion As String
                    Public Property CompilerVersion() As String
                        Get
                            Return Application.ProductVersion
                        End Get
                        Set(ByVal value As String)
                        End Set
                    End Property

                End Class
                Public Release As New clsRelease

            End Class
            Public Attached As New clsAttached

        End Class

        Public Releases As clsReleases


        ' 5.12 - Colophon

        ' 5.13 - Annotation

        ' 5.14 - Examples    

    End Class

End Class
