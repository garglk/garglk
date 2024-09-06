Imports System.Text.Encoding
Imports System.Xml


Public Class clsBlorb

    Private Shared iReadOffset As Integer = 0
    Private Shared stmBlorb As IO.FileStream
    Private Shared BlorbOffset As Long ' Where does the Blorb start within the file

    Friend Shared ExecResource As Byte()
    Friend Shared bObfuscated As Boolean = True
    Friend Shared ExecType As String = Nothing
    'Friend Shared ImageResources As New Dictionary(Of Integer, Image)       ' - These are just a local cache for Runner
    Friend Shared SoundResources As New Dictionary(Of Integer, SoundFile)   ' /
    Friend Shared MetaData As System.Xml.XmlDocument
    Friend Shared Frontispiece As Integer = -1
    Friend Shared ResourceIndex As New Dictionary(Of String, UInt32)
    Friend Shared sFilename As String
    Friend Shared bEXE As Boolean = False
    Friend Shared Data As String
    Friend Shared DataType As String

    Private Shared cnkFORM As FormChunk

#If Not Adravalon Then
    Public Function GetImage(ByVal iResourceNumber As Integer, Optional ByVal bStore As Boolean = False, Optional ByRef sExtn As String = Nothing) As Image
        Dim bStreamOpen As Boolean = stmBlorb.CanRead

        Try
            If Not ImageResources.ContainsKey(iResourceNumber) Then
                If ResourceIndex.ContainsKey("Pict" & iResourceNumber) Then
                    Dim iOffset As UInt32 = ResourceIndex("Pict" & iResourceNumber)
                    Dim cnkImage As New PictResourceChunk

                    If Not bStreamOpen Then stmBlorb = New IO.FileStream(sFilename, IO.FileMode.Open, IO.FileAccess.Read)
                    stmBlorb.Position = iOffset + BlorbOffset

                    If cnkImage.LoadChunk Then
                        sExtn = cnkImage.img.sExtn
                        If Not bStore Then Return cnkImage.img.Image
                        ImageResources.Add(iResourceNumber, cnkImage.img.Image)
                    End If
                End If
            End If
            If ImageResources.ContainsKey(iResourceNumber) Then Return ImageResources(iResourceNumber)

        Catch ex As Exception
            ErrMsg("GetImage error", ex)
        Finally
            If Not bStreamOpen Then stmBlorb.Close()
        End Try
        Return Nothing

    End Function
#Else
    Public Function GetImage(ByVal iResourceNumber As Integer, Optional ByVal bStore As Boolean = False, Optional ByRef sExtn As String = Nothing) As Byte()
        Dim bStreamOpen As Boolean = stmBlorb.CanRead
        Try
            If ResourceIndex.ContainsKey("Pict" & iResourceNumber) Then
                Dim iOffset As UInt32 = ResourceIndex("Pict" & iResourceNumber)
                Dim cnkImage As New PictResourceChunk

                If Not bStreamOpen Then stmBlorb = New IO.FileStream(sFilename, IO.FileMode.Open, IO.FileAccess.Read)
                stmBlorb.Position = iOffset + BlorbOffset

                If cnkImage.LoadChunk Then
                    Return cnkImage.img.bytImage
                End If
            End If
        Catch ex As Exception
            ErrMsg("GetImage error", ex)
        Finally
            If Not bStreamOpen Then stmBlorb.Close()
        End Try
        Return Nothing
    End Function
#End If

    Friend Function GetSound(ByVal iResourceNumber As Integer, Optional ByVal bStore As Boolean = False, Optional ByRef sExtn As String = Nothing) As SoundFile

        Dim bStreamOpen As Boolean = stmBlorb.CanRead

        Try
            If Not SoundResources.ContainsKey(iResourceNumber) Then
                If ResourceIndex.ContainsKey("Snd " & iResourceNumber) Then
                    Dim iOffset As UInt32 = ResourceIndex("Snd " & iResourceNumber)
                    Dim cnkSound As New SoundResourceChunk

                    If Not bStreamOpen Then stmBlorb = New IO.FileStream(sFilename, IO.FileMode.Open, IO.FileAccess.Read)
                    stmBlorb.Position = iOffset + BlorbOffset

                    If cnkSound.LoadChunk Then
                        sExtn = cnkSound.snd.sExtn
                        If Not bStore Then Return cnkSound.snd
                        SoundResources.Add(iResourceNumber, cnkSound.snd)
                    End If
                End If
            End If
            If SoundResources.ContainsKey(iResourceNumber) Then Return SoundResources(iResourceNumber)

        Catch ex As Exception
            ErrMsg("GetSound error", ex)
        Finally
            If Not bStreamOpen Then stmBlorb.Close()
        End Try
        Return Nothing

    End Function

    Friend Class SoundFile ' Temp, for now 
        Public bytSound As Byte() = {}
        Public sExtn As String

        Public Function Save(ByVal sFilename As String) As Boolean
            Dim fs As New IO.FileStream(sFilename, IO.FileMode.CreateNew)
            fs.Write(bytSound, 0, bytSound.Length - 1)
            fs.Close()
        End Function

    End Class

    Friend Class ImageFile
        Public bytImage As Byte() = {}
        Public sExtn As String

#If Not Adravalon Then
        Public ReadOnly Property Image As Image
            Get
                Dim msImage As New IO.MemoryStream(bytImage)
                Return New Bitmap(msImage)
            End Get
        End Property
#End If
    End Class

    Private MustInherit Class Chunk
        Public MustOverride Property ID As String

        Private iLength As UInt32
        Public Overridable Property Length As UInt32
            Get
                Return iLength
            End Get
            Set(value As UInt32)
                iLength = value
            End Set
        End Property


        Public Overridable Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            Try
                If iStartPos > -1 Then stmBlorb.Position = iStartPos

                ' First 4 bytes tell us what sort of chunk this is
                Dim bytID(3) As Byte
                iReadOffset += stmBlorb.Read(bytID, 0, 4)
                ID = UTF8.GetString(bytID)

                ' Next 4 bytes tell us the size of the chunk
                Dim bytSize(3) As Byte
                iReadOffset += stmBlorb.Read(bytSize, 0, 4)
                iLength = ByteToInt(bytSize)

                Return True
            Catch ex As Exception
                Return False
            End Try

        End Function

        Public Sub SkipPadding()
            If stmBlorb.Position Mod 2 = 1 Then stmBlorb.Position += 1
        End Sub

        Public Sub WritePadding()
            If stmBlorb.Position Mod 2 = 1 Then stmBlorb.WriteByte(CByte(0))
        End Sub

        Public Chunks As New List(Of Chunk)
        Public Overridable Function WriteChunk() As Boolean
            Try
                stmBlorb.Write(UTF8.GetBytes(ID), 0, 4) ' Chunk Type
                stmBlorb.Write(IntToByte(Length), 0, 4) ' Size

                Return True
            Catch ex As Exception
                Return False
            End Try
        End Function

    End Class


    Private Class SkipChunk
        Inherits Chunk

        Private bKnown As Boolean = True
        Public Sub New(Optional ByVal bKnown As Boolean = True)
            Me.bKnown = bKnown
        End Sub

        Dim sID As String
        Public Overrides Property ID As String
            Get
                Return sID
            End Get
            Set(value As String)
                sID = value
            End Set
        End Property

        Public Overrides Function LoadChunk(Optional iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            If Not bKnown Then
#If DEBUG Then
                ErrMsg("Skipping unknown chunk type """ & ID & """")
#End If
            End If


            stmBlorb.Position += Me.Length

            SkipPadding()

            Return True
        End Function
    End Class

    Private Class FrontispieceChunk
        Inherits Chunk

        Public Overrides Property ID As String
            Get
                Return "Fspc"
            End Get
            Set(value As String)
                If value <> "Fspc" Then Throw New Exception("Bad ID in Frontispiece Chunk: " & value)
            End Set
        End Property

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            ' Number of a Pict resource
            Dim bytSize(3) As Byte
            iReadOffset += stmBlorb.Read(bytSize, 0, 4)
            Frontispiece = CInt(ByteToInt(bytSize))

            SkipPadding()

            Return True
        End Function

        Public Overrides Function WriteChunk() As Boolean

            If Frontispiece > -1 Then
                Length = CUInt(4)
                If Not MyBase.WriteChunk Then Return False

                stmBlorb.Write(IntToByte(CUInt(Frontispiece)), 0, 4) ' Resource Number

                WritePadding()
            End If
            Return True
        End Function
    End Class


    Private Class MetaDataChunk
        Inherits Chunk

        Public Overrides Property ID As String
            Get
                Return "IFmd"
            End Get
            Set(value As String)
                If value <> "IFmd" Then Throw New Exception("Bad ID in Metadata Chunk: " & value)
            End Set
        End Property

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            Dim bytMeta(CInt(Length) - 1) As Byte
            iReadOffset += stmBlorb.Read(bytMeta, 0, CInt(Length))
            Dim sXML As String = UTF8.GetString(bytMeta)

            Dim xmlMeta As New XmlDocument
            xmlMeta.LoadXml(sXML)
            MetaData = xmlMeta

            SkipPadding()

            Return True
        End Function

        Public Overrides Function WriteChunk() As Boolean

            If MetaData IsNot Nothing Then
                Length = CUInt(UTF8.GetBytes(MetaData.OuterXml).Length) ' UTC bytes may be different from character count
                If Not MyBase.WriteChunk Then Return False

                stmBlorb.Write(UTF8.GetBytes(MetaData.OuterXml), 0, CInt(Length))

                WritePadding()
            End If
            Return True
        End Function

    End Class

    Private Class DataChunk
        Inherits Chunk

        Private sID As String
        Public Overrides Property ID As String
            Get
                If sID Is Nothing Then sID = "TEXT"
                Return sID
            End Get
            Set(value As String)
                Select Case value
                    Case "TEXT", "BINA"
                        sID = value
                    Case Else
                        Throw New Exception("Bad ID in Data Resource Chunk: " & value)
                End Select
            End Set
        End Property

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            Dim bytData(CInt(Length) - 1) As Byte
            iReadOffset += stmBlorb.Read(bytData, 0, CInt(Length))
            Dim sData As String = UTF8.GetString(bytData)
            DataType = sData.Substring(0, 4)
            Data = sData.Substring(4)
            
            SkipPadding()

            Select Case DataType
                Case "RLAY"
                    ' Restore Runner Layout
                    Dim sIFID As String = ""
                    If MetaData IsNot Nothing Then
                        sIFID = MetaData.GetElementsByTagName("ifid").Item(0).InnerText
                        If sIFID <> "" Then sIFID = "-" & sIFID
                    End If
                    Dim sXMLFile As String = DataPath(True) & "RunnerLayout" & sIFID & ".xml"

                    If Not IO.File.Exists(sXMLFile) Then
                        IO.File.WriteAllText(sXMLFile, Data)
                    End If
            End Select

            Return True
        End Function

        Public Overrides Function WriteChunk() As Boolean

            If DataType <> "" Then
                Dim bytData As Byte() = UTF8.GetBytes(DataType & Data)
                Length = CUInt(bytData.Length) ' UTC bytes may be different from character count
                If Not MyBase.WriteChunk Then Return False

                stmBlorb.Write(bytData, 0, CInt(Length))

                WritePadding()
            End If
            Return True
        End Function

    End Class


    Private Class ExecResourceChunk
        Inherits Chunk

        Private sID As String
        Public Overrides Property ID As String
            Get
                Return sID
            End Get
            Set(value As String)
                Select Case value
                    Case "ZCOD", "GLUL", "TAD2", "TAD3", "HUGO", "ALAN", "ADRI", "LEVE", "AGT ", "MAGS", "ADVS", "EXEC"
                        sID = value
                    Case Else
                        Throw New Exception("Bad ID in Exec Resource Chunk: " & value)
                End Select
            End Set
        End Property

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            clsBlorb.ExecType = sID

            Dim bytExec(CInt(Length) - 1) As Byte
            iReadOffset += stmBlorb.Read(bytExec, 0, CInt(Length))
            clsBlorb.ExecResource = bytExec
            SkipPadding()

            Return True
        End Function

        Public Overrides Function WriteChunk() As Boolean

            If ExecResource IsNot Nothing Then
                If Not MyBase.WriteChunk Then Return False

                stmBlorb.Write(ExecResource, 0, ExecResource.Length)

                WritePadding()
            End If
            Return True
        End Function

    End Class


    Private Class PictResourceChunk
        Inherits Chunk

        Private sID As String
        Public Overrides Property ID As String
            Get
                If img IsNot Nothing Then
                    Select Case img.sExtn
                        Case "jpeg", "jpg"
                            Return "JPEG"
                        Case "png"
                            Return "PNG "
                        Case "gif"
                            Return "GIF " ' Not valid Blorb
                        Case Else
                            ErrMsg("Blorb does not support " & img.sExtn & " format")
                    End Select
                    Return Nothing
                Else
                    Return sID
                End If
            End Get
            Set(value As String)
                Select Case value
                    Case "PNG ", "JPEG", "GIF "
                        sID = value
                    Case Else
                        Throw New Exception("Bad ID in Picture Resource Chunk: " & value)
                End Select
            End Set
        End Property

        Public img As ImageFile


        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            Dim bytPict(CInt(Length) - 1) As Byte
            iReadOffset += stmBlorb.Read(bytPict, 0, CInt(Length))

            Dim sExtn As String = ""
            Select Case ID
                Case "GIF "
                    sExtn = "gif" ' Not valid Blorb
                Case "PNG "
                    sExtn = "png"
                Case "JPEG"
                    sExtn = "jpg"
            End Select

            img = New ImageFile
            img.bytImage = bytPict
            img.sExtn = sExtn

            SkipPadding()

            Return True
        End Function


        Public Overrides Function WriteChunk() As Boolean
            If Not MyBase.WriteChunk Then Return False
            stmBlorb.Write(img.bytImage, 0, img.bytImage.Length)

            WritePadding()
            Return True
        End Function

    End Class


    Private Class SoundResourceChunk
        Inherits Chunk

        Private sID As String
        Public Overrides Property ID As String
            Get
                If snd IsNot Nothing Then
                    Select Case snd.sExtn
                        Case "mp3"
                            Return "MP3 " ' Not valid Blorb
                        Case "wav"
                            Return "WAVE" ' Not valid Blorb
                        Case "mid"
                            Return "MIDI" ' Not valid Blorb
                        Case "aiff", "aif"
                            Return "AIFF"
                        Case "ogg"
                            Return "OGGV"
                        Case "mod"
                            Return "MOD "
                    End Select
                End If
                Return sID
            End Get
            Set(value As String)
                Select Case value
                    Case "AIFF", "OGGV", "MOD "
                        sID = value
                    Case "MP3 ", "WAVE", "MIDI"
                        sID = value ' Tho not valid Blorb
                    Case Else
                        Throw New Exception("Bad ID in Sound Resource Chunk: " & value)
                End Select
            End Set
        End Property

        Public snd As SoundFile


        Public Sub SetSound(ByVal sSound As String)
            Try
                If IO.File.Exists(sSound) Then
                    Length = CUInt(FileLen(sSound))
                    Dim fs As New IO.FileStream(sSound, IO.FileMode.Open, IO.FileAccess.Read)
                    snd = New SoundFile
                    ReDim snd.bytSound(CInt(Length) - 1)
                    fs.Read(snd.bytSound, 0, CInt(Length))
                    snd.sExtn = IO.Path.GetExtension(sSound).ToLower.Substring(1)
                    fs.Close()
                End If
            Catch ex As Exception
                ErrMsg("Error storing sound", ex)
            End Try
        End Sub


        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            Dim bytSound(CInt(Length) - 1) As Byte
            iReadOffset += stmBlorb.Read(bytSound, 0, CInt(Length))

            snd = New SoundFile
            snd.bytSound = bytSound

            With snd
                Dim msImage As New IO.MemoryStream(bytSound)
                Select Case ID
                    Case "AIFF"
                        .sExtn = "aif"
                    Case "OGGV"
                        .sExtn = "ogg"
                    Case "MOD "
                        .sExtn = "mod"
                    Case "MP3 "
                        .sExtn = "mp3"
                    Case "MIDI"
                        .sExtn = "mid"
                    Case "WAVE"
                        .sExtn = "wav"
                End Select
            End With

            SkipPadding()

            Return True
        End Function

        Public Overrides Function WriteChunk() As Boolean
            If Not MyBase.WriteChunk Then Return False

            stmBlorb.Write(snd.bytSound, 0, snd.bytSound.Length)

            WritePadding()
            Return True
        End Function

    End Class


    Private Class ResourceIndexChunk
        Inherits Chunk

        Public Overrides Property ID As String
            Get
                Return "RIdx"
            End Get
            Set(value As String)
                If value <> "RIdx" Then Throw New Exception("Bad ID in Resource Index Chunk: " & value)
            End Set
        End Property

        Public ReadOnly Property NumberOfResources As Integer
            Get
                Return clsBlorb.ResourceIndex.Count
            End Get
        End Property


        Public Overrides Property Length As UInteger
            Get
                Return CUInt(4 + (12 * NumberOfResources))
            End Get
            Set(value As UInteger)
                MyBase.Length = value
            End Set
        End Property


        Friend Class ResourceIndex
            Private sUsage As String
            Public Property Usage As String
                Get
                    Return sUsage
                End Get
                Set(value As String)
                    Select Case value
                        Case "Pict", "Snd ", "Exec"
                            sUsage = value
                        Case Else
                            Throw New Exception("Unknown Resource Usage: " & value)
                    End Select
                End Set
            End Property

            Private iNumber As UInt32 ' Number of resource
            Public Property Number As UInt32
                Get
                    Return iNumber
                End Get
                Set(value As UInt32)
                    iNumber = value
                End Set
            End Property

            Private iStart As UInt32 ' Starting position of resource
            Public Property Start As UInt32
                Get
                    Return iStart
                End Get
                Set(value As UInt32)
                    iStart = value
                End Set
            End Property
        End Class

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            ' Next 4 bytes tell us the number of resources in the index
            Dim bytNum(3) As Byte
            iReadOffset += stmBlorb.Read(bytNum, 0, 4)
            Dim iNumberOfResources As Integer = CInt(ByteToInt(bytNum))

            For iResource As Integer = 0 To iNumberOfResources - 1

                Dim sKey As String
                Dim bytUsage(3) As Byte
                iReadOffset += stmBlorb.Read(bytUsage, 0, 4)
                sKey = UTF8.GetString(bytUsage)

                Dim btyNumber(3) As Byte
                iReadOffset += stmBlorb.Read(btyNumber, 0, 4)
                sKey &= ByteToInt(btyNumber)

                Dim bytStart(3) As Byte
                iReadOffset += stmBlorb.Read(bytStart, 0, 4)

                clsBlorb.ResourceIndex.Add(sKey, ByteToInt(bytStart))
            Next

            SkipPadding()

            Return True
        End Function


        Public Overrides Function WriteChunk() As Boolean
            If Not MyBase.WriteChunk Then Return False

            stmBlorb.Write(IntToByte(CUInt(clsBlorb.ResourceIndex.Count)), 0, 4)

            For Each sKey As String In clsBlorb.ResourceIndex.Keys
                stmBlorb.Write(UTF8.GetBytes(sKey.Substring(0, 4)), 0, 4)
                stmBlorb.Write(IntToByte(CUInt(sKey.Substring(4))), 0, 4)
                stmBlorb.Write(IntToByte(clsBlorb.ResourceIndex(sKey)), 0, 4)
            Next

            Return True

        End Function

    End Class


    Private Class FormChunk
        Inherits Chunk
        Public Overrides Property ID As String
            Get
                Return "FORM"
            End Get
            Set(value As String)
                If value <> "FORM" Then Throw New Exception("Bad ID in Form Chunk: " & value)
            End Set
        End Property

        Public Overrides Property Length As UInteger
            Get
                Dim iLength As UInt32 = 0
                For Each c As Chunk In Chunks
                    iLength += c.Length
                Next
                Return iLength
            End Get
            Set(value As UInteger)
                MyBase.Length = value
            End Set
        End Property
        Private sSubTypeID As String
        Public Property SubTypeID As String
            Get
                Return sSubTypeID
            End Get
            Set(value As String)
                sSubTypeID = value
            End Set
        End Property

        Public cnkResourceIndex As ResourceIndexChunk

        Public Overrides Function LoadChunk(Optional ByVal iStartPos As Integer = -1) As Boolean
            If Not MyBase.LoadChunk(iStartPos) Then Return False

            ' Next 4 bytes tell us the FORM type
            Dim bytID(3) As Byte
            iReadOffset += stmBlorb.Read(bytID, 0, 4)
            SubTypeID = UTF8.GetString(bytID)

            cnkResourceIndex = New ResourceIndexChunk
            If Not cnkResourceIndex.LoadChunk() Then Return False

            Dim iStreamLength As Long = stmBlorb.Length
            If bEXE Then iStreamLength -= 6
            While stmBlorb.Position < iStreamLength
                Dim cnk As Chunk = Nothing

                ' Peek at the next 4 bytes to work out what chunk type it is
                Dim bytNext(3) As Byte
                iReadOffset = CInt(stmBlorb.Position)
                iReadOffset += stmBlorb.Read(bytNext, 0, 4)
                Select Case UTF8.GetString(bytNext)
                    Case "ZCOD", "GLUL", "TAD2", "TAD3", "HUGO", "ALAN", "ADRI", "LEVE", "AGT ", "MAGS", "ADVS", "EXEC"
                        cnk = New ExecResourceChunk
                    Case "GIF ", "PNG ", "JPEG"
                        cnk = New SkipChunk
                    Case "AIFF", "OGGV", "MOD ", "MP3 ", "WAVE", "MIDI"
                        cnk = New SkipChunk
                    Case "IFmd"
                        cnk = New MetaDataChunk
                    Case "Fspc"
                        cnk = New FrontispieceChunk
                    Case "Plte", "IFhd" ' Colour Palette, Game Identifier
                        cnk = New SkipChunk ' Just ignore them
                    Case "TEXT", "BINA"
                        cnk = New DataChunk
                    Case Else
                        cnk = New SkipChunk(False)
                End Select
                If cnk Is Nothing OrElse Not cnk.LoadChunk(iReadOffset - 4) Then Return False
            End While

            SkipPadding()

            Return True
        End Function


        Public Overrides Function WriteChunk() As Boolean

            ' Gotta write the resources to the index so we can calculate the size of the index resource
            Dim iResource As UInt32 = 0
            For Each c As Chunk In Chunks
                Dim sKey As String = ""
                Select Case True
                    Case TypeOf c Is ExecResourceChunk
                        sKey = "Exec"
                    Case TypeOf c Is PictResourceChunk
                        sKey = "Pict"
                    Case TypeOf c Is SoundResourceChunk
                        sKey = "Snd "
                End Select
                If sKey <> "" Then
                    clsBlorb.ResourceIndex.Add(sKey & iResource, 0)
                    iResource = CUInt(iResource + 1)
                End If
            Next

            ' Then write out the resource lengths
            Dim iOffset As UInt32 = 12
            iResource = 0
            For Each c As Chunk In Chunks
                iOffset = CUInt(iOffset)
                Dim sKey As String = ""
                Select Case True
                    Case TypeOf c Is ExecResourceChunk
                        sKey = "Exec"
                    Case TypeOf c Is PictResourceChunk
                        sKey = "Pict"
                    Case TypeOf c Is SoundResourceChunk
                        sKey = "Snd "
                End Select
                If sKey <> "" Then
                    clsBlorb.ResourceIndex(sKey & iResource) = iOffset
                    iResource = CUInt(iResource + 1)
                End If
                iOffset = CUInt(iOffset + c.Length + 8)
                If iOffset Mod 2 = 1 Then iOffset = CUInt(iOffset + 1)
            Next

            If Not MyBase.WriteChunk() Then Return False
            stmBlorb.Write(UTF8.GetBytes("IFRS"), 0, 4)

            For Each c As Chunk In Chunks
                If Not c.WriteChunk() Then Return False
            Next


            WritePadding()
            Return True

        End Function
    End Class

    Private Sub ClearBlorb()
        ExecType = Nothing
        ExecResource = Nothing
        'ImageResources.Clear()
        SoundResources.Clear()
        ResourceIndex.Clear()
        MetaData = Nothing
        Frontispiece = -1
    End Sub

    Public Function LoadBlorb(ByVal stmBlorb As IO.FileStream, ByVal sFilename As String, Optional ByVal BlorbOffset As Long = 0) As Boolean
        ClearBlorb()
        clsBlorb.BlorbOffset = BlorbOffset
        clsBlorb.sFilename = sFilename
        clsBlorb.stmBlorb = stmBlorb
        iReadOffset = 0
        cnkFORM = New FormChunk
        Return cnkFORM.LoadChunk()
    End Function

    Friend Function CurrentIFID() As String

        If Adventure IsNot Nothing AndAlso Adventure.BabelTreatyInfo IsNot Nothing AndAlso Adventure.BabelTreatyInfo.Stories.Length = 1 Then
            Return "-" & Adventure.BabelTreatyInfo.Stories(0).Identification.IFID
        End If
        Return ""

    End Function

    Private Shared Function ByteToInt(ByVal byt() As Byte) As UInt32
        ' Reverse the bytes
        Dim bytRev(byt.Length - 1) As Byte
        For i As Integer = 0 To byt.Length - 1
            bytRev(i) = byt(byt.Length - 1 - i)
        Next
        Return BitConverter.ToUInt32(bytRev, 0)
    End Function

    Private Shared Function IntToByte(ByVal int As UInt32) As Byte()
        Dim bytRev(3) As Byte
        Dim byt(3) As Byte
        bytRev = BitConverter.GetBytes(int)
        For i As Integer = 0 To byt.Length - 1
            byt(i) = bytRev(bytRev.Length - 1 - i)
        Next
        Return byt
    End Function

End Class
