# Automatically run ERC/DRC and generate KiCad project assets for release and bundle them into a zip bundle.
# 
# Generates the following:
#    - Schematics
#    - PCBA BOM and Hand Assembly BOM (e.g. through-hole components)
#    - Gerbers, Drills/Map, and POS
#    - Interactive BOM (iBOM)

#$ibom = $null             								# Filepath to generate_interactive_bom.py
#$kicad_cli = $null        								# Filepath to kicad-cli.exe

$boards = "./kicad"           						    # KiCad project source directory
$boards_config_name = "board.config.json"
$package_name = "SW32" 									# Nested zip bundle of all generated board files

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

# If git is available and this folder is a git repository, get short SHA1 hash of HEAD
$repo_hash = $null
if ((Test-Path -Path ".git/") -and ($null -ne (Get-Command git))) {
    $repo_hash = git rev-parse --short HEAD
    Write-Host "Hash @ HEAD: $($repo_hash)"
}

$generated_items = @()
$boms_bundle = @()
$pcbs_bundle = @()

# Find *.kicad_sch files that have the same name as their parent folder
Get-ChildItem -Path $boards -Recurse -Include '*.kicad_sch' | Where-Object { $_.BaseName -eq $_.Directory.Name } | ForEach-Object {
    $boards_bundle = @()

    $name = $_.Basename

    $config = (Get-Content -Raw -Path (Join-Path -Path $_.Directory -ChildPath $boards_config_name) | ConvertFrom-Json)

    Write-Host "Processing $name..."
    Write-Host "  - Type: $($config.type)"
    Write-Host "  - Extra Layers: $($config.extra_layers)"
    Write-Host ""
	
    # TODO: Run an ERC before generating Schematics
    # kicad-cli sch erc --exit-code-violations --severity-error --severity-warning "$sch_file"
		
    # Generate schematic
    Write-Host "Generating schematic..."
    $schematic = Join-Path -Path $_.Directory -ChildPath "$name.pdf" 
    $boards_bundle += $schematic
    
    $arguments = "sch export pdf --output ""$schematic"" -D ""hash=$repo_hash"" ""$_"""
    Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments

    # Generate bill of materials (BOM)
    Write-Host "Generating BOM..."
    $raw_bom = Join-Path -Path $_.Directory -ChildPath "$name.all.csv"

    $fields = 'Value,Reference,Footprint,LCSC,Service,${QUANTITY},${DNP}'
    $labels = 'Comment,Designator,Footprint,LCSC Part Number,Service,Qty,DNP'
    $groupby = 'Value,Footprint,LCSC,Service,${DNP}'
  
    $arguments = "sch export bom --output ""$raw_bom"" --fields ""$fields"" --labels ""$labels"" --group-by ""$groupby"" --ref-range-delimiter """" ""$_"""
    Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
    Write-Host "Generated to '$($raw_bom)'."

    # Include raw bom in BOMs and board bundle
    $boms_bundle += $raw_bom

    $CSV = Import-Csv -Path $raw_bom
    
    # Export JLC PCBA bom - ignore components marked DNP or components marked for assembly by JLCPCB assembly service
    $pcba_bom = Join-Path -Path $_.Directory -ChildPath "$name.pcba.csv"
    ($CSV | Where-Object { ( $_.Service -Match 'PCBA') -and ( '' -eq $_.DNP ) } | Select-Object Comment, Designator, Footprint, "LCSC Part Number" | Export-Csv -NoTypeInformation -Path $pcba_bom)
 
    # Include PCBA bom in board bundle
    $boards_bundle += $pcba_bom

    # Export hand assembly (through hole) bom
    $hand_bom = Join-Path -Path $_.Directory -ChildPath "$name.hand_assembly.csv"
    ($CSV | Where-Object { ( $_.Service -Match 'HandAssembly') } | Export-Csv -NoTypeInformation -Path $hand_bom)
  
    # Include hand assembly bom in BOMs bundle
    $boms_bundle += $hand_bom

    # Check if the schematic has a PCB file
    $pcb_file = Join-Path -Path $_.Directory -ChildPath "$name.kicad_pcb"		       
    $gerbers = Join-Path -Path $_.Directory -ChildPath "gerbers"
    if (Test-Path -Path $pcb_file) {       
        New-Item -Force -ItemType Directory -Path $gerbers
        $generated_items += $gerbers
       
        # TODO: Run a DRC before generating gerbers
        # kicad-cli pcb drc --exit-code-violations --schematic-parity --severity-error --severity-warning "$pcb_file"
		
        # Generate the interactive BOM
        Write-Host "Generating iBOM..."
            
        # Generate iBOM for PCBA service (no Hand assembly components)
        $arguments = """$ibom"" --no-browser --extra-fields LCSC --include-nets --include-tracks --variant-field Service --variants-blacklist HandAssembly --name-format ""%f-pcba-ibom"" --dest-dir ./ --extra-data-file ""$pcb_file"" ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath "python" -ArgumentList $arguments

        $ibom_pcba = Join-Path -Path $_.Directory -ChildPath "$name-pcba-ibom.html"
        if (Test-Path -Path $ibom_pcba) {
            $boards_bundle += $ibom_pcba
        }
        
        # Generate full iBOM for BOMs bundle
        $arguments = """$ibom"" --no-browser --extra-fields LCSC,Service,`${DNP} --include-nets --include-tracks --variant-field Service --name-format ""%f-ibom"" --dest-dir ./ --extra-data-file ""$pcb_file"" ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath "python" -ArgumentList $arguments

        $ibom_full = Join-Path -Path $_.Directory -ChildPath "$name-ibom.html"
        if (Test-Path -Path $ibom_full) {
            $boms_bundle += $ibom_full
        }

        # Generate gerbers
        Write-Host "Generating gerbers..."
        $layers = "F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts,$($config.extra_layers)"

        $arguments = "pcb export gerbers --output ""$gerbers"" --disable-aperture-macros --no-x2 --subtract-soldermask -D hash=$repo_hash --layers ""$layers"" ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
  

        # Generate drills
        Write-Host "Generating drills..."
        $arguments = "pcb export drill --output ""$gerbers/"" --format excellon --drill-origin absolute --excellon-oval-format alternate -u mm --excellon-zeros-format decimal --generate-map --map-format gerberx2 ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
    		
        
        # Generate POS...
        Write-Host "Generating pos..."
        $pos = Join-Path -Path $gerbers -ChildPath "$name.pos"
        $arguments = "pcb export pos --output ""$pos"" --side front --format csv --units mm --smd-only --exclude-dnp ""$pcb_file"""
        Start-Process -Wait -NoNewWindow -FilePath $kicad_cli -ArgumentList $arguments
        Write-Host "Generated to '$($pos)'." 

        # Make POS file compliant with JLCSMT
        $CSV = Get-Content -Path $pos
        if ($CSV.Count -gt 1) {
            $CSV[0] = '"Designator","Val","Package","MidX","MidY","Rotation","Layer"'
            $CSV | Out-File -FilePath $pos
        }
				
        # Add gerber files to zip (including pos and drill files)
        $boards_bundle += (Get-ChildItem -Path "$gerbers/*")
    }

    # Track generated files
    $generated_items += $boards_bundle
    $generated_items += $boms_bundle

    Write-Host "Zipping board assets..."
    $board_bundle = Join-Path -Path $_.Directory -ChildPath "$name.zip"    
    $generated_items += $board_bundle

    $boards_bundle | Compress-Archive -Force -DestinationPath $board_bundle
   
    $pcbs_bundle += $board_bundle   

}

# Bundle all board zips into a single zip
Write-Host "Bundling assets to $package_name-*.zip ..."
$pcbs_bundle | Compress-Archive -Force -DestinationPath "$package_name-PCBs.zip"

# Bundle all BOMs into a single zip
$boms_bundle | Compress-Archive -Force -DestinationPath "$package_name-BOMs.zip"

# Delete generated files
Write-Host "Cleaning up..."
$generated_items | Sort-Object -Unique -Descending | ForEach-Object { 
    Write-Host "  - '$($_)'"
    if (Test-Path -Path $_) { Remove-Item $_ }
}


Write-Host ""
Write-Host "##########################################"
Write-Host "#####             Done.              #####"
Write-Host "##########################################"