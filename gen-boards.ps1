# Generates the following KiCad project assets:
#    - Schematics
#    - PCBA BOM and Hand Assembly BOM (e.g. through-hole components)
#    - Gerbers, Drills/Map, and CPL
#    - Interactive BOM (iBOM)
#
#
#	Requires KiCad 8 and the InteractiveHtmlBom plugin installed.
#
# The script requires access to `kicad-cli.exe`, it must be on PATH or manually set below.
# Script will automatically check in "<MyDocuments>/KiCad/8.0" for `generate_interactive_bom.py`
# or manually set below.

#$ibom = $null             								# Filepath to generate_interactive_bom.py
#$kicad_cli = $null        								# Filepath to kicad-cli.exe

$boards = "./kicad"           						    # KiCad project source directory

# Sanity check - Dont run if boards folder is missing
if (!(Test-Path -Path $boards)) {
    Write-Host "KiCad projects folder $boards does not exist!"
    exit
}

# We need KiCad Command Prompt for CLI and iBOM, so bootstrap this script with kicad-cmd.bat
$kicad_cli = if ($null -ne $kicad_cli ) { $kicad_cli } else { (Get-Command kicad-cli).Path }

if ($null -ne $kicad_cli) {
    if ($null -eq $env:KICAD_VERSION) {
        # Test if we are in the environment
        Write-Host "Not launched with environment, restarting with kicad-cmd.bat"

        # Locate the KiCad install directory from the kicad-cli.exe
        $kicad_path = if ($null -ne $kicad_path ) { $kicad_path }else { (Get-Item $kicad_cli).Directory.Parent.FullName }

        # Find kicad-cmd.bat in the KiCad install directory
        $kicad_cmd_path = Get-ChildItem -Path $kicad_path -Recurse -Depth 1 -Include 'kicad-cmd.bat'
    
        if ($null -ne $kicad_cmd_path) {
            &"cmd.exe" "/C" """$kicad_cmd_path"" & cd ""$PSScriptRoot"" & powershell -File ""$PSCommandPath"""
        }
        else {
            Write-Host "Failed to find kicad-cmd.bat! Are you sure KiCad 8.0 is installed?"
        }
        exit
    }
}
else {
    Write-Host "Failed to find kicad-cli.exe! Are you sure KiCad 8.0 is installed and added to PATH?"
    exit
}

# Try and find iBOM in Documents/KiCad
$documents = ([Environment]::GetFolderPath("MyDocuments"))
if ($null -eq $ibom) {
    $ibom = Get-ChildItem -Path (Join-Path -Path $documents -ChildPath "KiCad/$($Env:KICAD_VERSION)") -Depth 4 -Recurse -Include 'generate_interactive_bom.py'
}

$tmp = New-Item -ItemType Directory -Path "./temp"
$cpl_rotations = Import-Csv -Path "cpl_rotations.csv"

# Find *.kicad_sch files that have the same name as their parent folder
Get-ChildItem -Path $boards -Recurse -Include '*.kicad_sch' | Where-Object { $_.BaseName -eq $_.Directory.Name } | ForEach-Object {
    $name = $_.Basename
    
    Write-Host "Processing $name..."
    Write-Host ""

    $pcb_output = New-Item -Force -ItemType Directory -Path "./pcb/$name"

    # Generate schematic
    Write-Host "Generating schematic..."
    $schematic = Join-Path -Path "./schematics" -ChildPath "$name.pdf"       
    $arguments = "sch export pdf --output ""$schematic"" ""$_"""
    Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
   
    # Generate bill of materials (BOM)
    Write-Host "Generating BOM..."
    $raw_bom = Join-Path -Path $tmp -ChildPath "$name.all.csv"

    $fields = 'Value,Reference,Footprint,LCSC,Service,${QUANTITY},${DNP}'
    $labels = 'Comment,Designator,Footprint,LCSC Part Number,Service,Qty,DNP'
    $groupby = 'Value,Footprint,LCSC,Service,${DNP}'

    $arguments = "sch export bom --output ""$raw_bom"" --fields ""$fields"" --labels ""$labels"" --group-by ""$groupby"" --format-preset CSV ""$_"""
    Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
    Write-Host "Generated to '$raw_bom'."

    $BOM = Import-Csv -Path $raw_bom    

    # Export JLC PCBA bom - ignore components marked DNP or components marked for hand assembly
    $bom_pcba = Join-Path -Path $pcb_output -ChildPath "$name.bom.csv"
    ($BOM | Where-Object { ( $_.Service -Match 'PCBA') -and ( '' -eq $_.DNP ) } | Select-Object Comment, Designator, Footprint, "LCSC Part Number" | Export-Csv -NoTypeInformation -Path $bom_pcba)

    if ((Get-Content -Path $bom_pcba).Count -eq 0) {
        Remove-Item -Path $bom_pcba
    }

    # Export hand assembly (through hole) bom
    $bom_hand = Join-Path -Path $pcb_output -ChildPath "$name.bom-hand_assembly.csv"
    ($BOM | Where-Object { ( $_.Service -Match 'HandAssembly') } | Export-Csv -NoTypeInformation -Path $bom_hand)


    # Check if the schematic has a PCB file
    $pcb_file = Join-Path -Path $_.Directory -ChildPath "$name.kicad_pcb"		       
    if (Test-Path -Path $pcb_file) {    
        $gerbers = New-Item -ItemType Directory -Force -Path "$pcb_output/gerbers"  
   
        if ($null -ne $ibom) {
            # Generate the interactive BOM
            Write-Host "Generating iBOM..."
            
            # Generate full iBOM
						#$ibom_dest_dir = Join-Path -Path "../.." -ChildPath $pcb_output
            $arguments = """$ibom"" --no-browser --extra-fields LCSC,Service,kicad_dnp --include-nets --include-tracks --variant-field Service --name-format ""%f-ibom"" --dest-dir ""../../pcb/${name}"" --extra-data-file ""$pcb_file"" ""$pcb_file"""
            Start-Process -Wait -NoNewWindow -FilePath "python" -ArgumentList $arguments
        }

        # Generate gerbers
        Write-Host "Generating gerbers..."
	
        # Include inner layers "In*.Cu" if found in kicad_pcb file
        $extra_layers = ""				
        $groups = (Select-String -Path $pcb_file -Pattern '\(\d{1,2}\s\"(?<name>In\d{1,2}\.Cu)\"\s\w+\)').Matches.Groups

        if ($groups -ne $null) {
            $extra_layers = ($groups | Where-Object { ($_.Name -eq 'name') } | Select-Object -Expand Value) -join ","				
        }

        Write-Host "Extra Layers: '$extra_layers'"
        $layers = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts,$extra_layers"

        $arguments = "pcb export gerbers --output ""$gerbers"" --disable-aperture-macros --no-x2 --subtract-soldermask --layers ""$layers"" ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments

        # Generate drills
        Write-Host "Generating drills..."
        $arguments = "pcb export drill --output ""$gerbers/"" --format excellon --drill-origin absolute --excellon-oval-format alternate -u mm --excellon-zeros-format decimal --generate-map --map-format gerberx2 ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
    
        # Generate CPL...
        Write-Host "Generating CPL..."
        $cpl = Join-Path -Path $pcb_output -ChildPath "$name.top-cpl.csv"
        $arguments = "pcb export pos --output ""$cpl"" --side front --format csv --units mm --smd-only --exclude-dnp ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
        Write-Host "Generated to '$cpl'." 

        # Make CPL file compliant with JLCSMT
        $CSV = Get-Content -Path $cpl
        if ($CSV.Count -gt 1) {
            $CSV[0] = '"Designator","Val","Package","Mid X","Mid Y","Rotation","Layer"'
            $CSV | Out-File -FilePath $cpl
           
            $CSV = Import-Csv -Path $cpl
            foreach ($cpl_item in $CSV) {
                $match = $cpl_rotations | Where-Object { $cpl_item.Package -Match $_.PackagePattern } | Select-Object -First 1
             
                if ($null -ne $match) {
                    $cpl_item.Rotation = (([double] $cpl_item.Rotation) + ([double]$match.RotationOffset)) % 360.0

                    if ($null -ne $match.OffsetX) {
                        $cpl_item."Mid X" = ([double] $cpl_item."Mid X") + ([double] $match.OffsetX)
                    }
                    if ($null -ne $match.OffsetY) {
                        $cpl_item."Mid Y" = ([double] $cpl_item."Mid Y") + ([double] $match.OffsetY)
                    }
                }
            }

            $CSV | Export-Csv -NoTypeInformation -Path $cpl

        }
        else {
            Remove-Item -Path $cpl
        }


        Get-ChildItem -Path "$gerbers/*" | Compress-Archive -Force -DestinationPath "$pcb_output/$name.zip"
        Remove-Item -Recurse -Path $gerbers  
    }
}


Remove-Item -Recurse -Path $tmp