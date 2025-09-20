// Tags Management Module
class TagsManager {
    constructor() {
        this.tags = [];
        this.filteredTags = [];
        this.currentSort = { field: 'name', direction: 'asc' };
        this.editingTag = null;
        this.currentTagType = null; // 'instrument' or 'controller'
    }

    init() {
        console.log('🚀 Initializing TagsManager...');
        this.setupEventListeners();
    }

    setupEventListeners() {
        console.log('🔧 Setting up event listeners...');
        
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => {
                console.log('📄 DOM loaded, setting up event listeners again...');
                this.setupEventListenersInternal();
            });
        } else {
            this.setupEventListenersInternal();
        }
    }
    
    setupEventListenersInternal() {
        console.log('🔧 Setting up internal event listeners...');
        
        // Search functionality
        const searchInput = document.getElementById('tag-search');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                this.filterTags();
            });
        }

        // Filter dropdowns
        const typeFilter = document.getElementById('tag-type-filter');
        const statusFilter = document.getElementById('tag-status-filter');
        
        if (typeFilter) {
            typeFilter.addEventListener('change', () => this.filterTags());
        }
        
        if (statusFilter) {
            statusFilter.addEventListener('change', () => this.filterTags());
        }

        // Refresh button
        const refreshBtn = document.getElementById('refresh-tags');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.loadTags());
        }

        // Tag Type Selection Cards - IMPROVED
        setTimeout(() => {
            const typeCards = document.querySelectorAll('.tag-type-card');
            console.log(`🎯 Found ${typeCards.length} tag type cards`);
            
            if (typeCards.length === 0) {
                console.warn('⚠️ No tag type cards found! Modal might not be loaded yet.');
                // Try again after a delay
                setTimeout(() => this.setupTagTypeCardListeners(), 1000);
            } else {
                this.setupTagTypeCardListeners();
            }
        }, 100);

        // Modal form handling
        const saveInstrumentBtn = document.getElementById('saveInstrument');
        if (saveInstrumentBtn) {
            saveInstrumentBtn.addEventListener('click', () => this.saveInstrument());
        }

        const saveControllerBtn = document.getElementById('saveController');
        if (saveControllerBtn) {
            saveControllerBtn.addEventListener('click', () => this.saveController());
        }

        // Auto-fill table names based on tag name
        this.setupAutoFillListeners();

        // Form validation
        this.setupFormValidation();
    }
    
    setupTagTypeCardListeners() {
        const typeCards = document.querySelectorAll('.tag-type-card');
        console.log(`🎯 Setting up listeners for ${typeCards.length} tag type cards`);
        
        typeCards.forEach((card, index) => {
            const type = card.getAttribute('data-type');
            console.log(`🏷️ Card ${index}: type="${type}"`);
            
            // Remove any existing listeners
            const newCard = card.cloneNode(true);
            card.parentNode.replaceChild(newCard, card);
            
            // Add new listener
            newCard.addEventListener('click', (e) => {
                e.preventDefault();
                e.stopPropagation();
                console.log(`🖱️ Card clicked! Type: ${type}`);
                this.openTagForm(type);
            });
            
            // Make it obvious it's clickable
            newCard.style.cursor = 'pointer';
            newCard.addEventListener('mouseenter', () => {
                newCard.style.transform = 'scale(1.05)';
                newCard.style.transition = 'transform 0.2s';
            });
            newCard.addEventListener('mouseleave', () => {
                newCard.style.transform = 'scale(1)';
            });
        });
        
        console.log('✅ Tag type card listeners set up successfully');
    }

    openTagForm(type) {
        console.log(`🚪 Opening tag form for type: ${type}`);
        this.currentTagType = type;
        
        // Close type selection modal
        const typeModalElement = document.getElementById('tagTypeModal');
        console.log(`🔍 Type modal element:`, typeModalElement);
        
        const typeModal = bootstrap.Modal.getInstance(typeModalElement);
        console.log(`🔍 Type modal instance:`, typeModal);
        
        if (typeModal) {
            console.log('🔒 Hiding type modal...');
            typeModal.hide();
        } else {
            console.warn('⚠️  No type modal instance found');
        }

        // Open appropriate form modal
        setTimeout(() => {
            if (type === 'instrument') {
                console.log('🔧 Opening instrument modal...');
                const instrumentModalElement = document.getElementById('instrumentModal');
                console.log(`🔍 Instrument modal element:`, instrumentModalElement);
                
                if (instrumentModalElement) {
                    const modal = new bootstrap.Modal(instrumentModalElement);
                    modal.show();
                    this.resetInstrumentForm();
                    console.log('✅ Instrument modal opened');
                } else {
                    console.error('❌ Instrument modal not found!');
                }
            } else if (type === 'controller') {
                console.log('⚙️  Opening controller modal...');
                const controllerModalElement = document.getElementById('controllerModal');
                console.log(`🔍 Controller modal element:`, controllerModalElement);
                
                if (controllerModalElement) {
                    const modal = new bootstrap.Modal(controllerModalElement);
                    modal.show();
                    this.resetControllerForm();
                    console.log('✅ Controller modal opened');
                } else {
                    console.error('❌ Controller modal not found!');
                }
            }
        }, 300);
    }

    detectTagType(tagName) {
        const name = tagName.toUpperCase();
        
        // Instrument prefixes
        if (name.startsWith('ET_') || name.startsWith('FIT_') || 
            name.startsWith('TIT_') || name.startsWith('PIT_') || 
            name.startsWith('LIT_') || name.startsWith('PDIT_')) {
            return 'instrument';
        }
        
        // Controller prefixes
        if (name.startsWith('FRC_') || name.startsWith('TRC_') || name.startsWith('PRC_')) {
            return 'controller';
        }
        
        return 'unknown';
    }

    setupAutoFillListeners() {
        // Instrument form auto-fill
        const instName = document.getElementById('inst_name');
        if (instName) {
            instName.addEventListener('input', (e) => {
                const name = e.target.value.toUpperCase();
                const valueTable = document.getElementById('inst_value_table');
                const alarmTable = document.getElementById('inst_alarm_table');
                
                if (name && valueTable && alarmTable) {
                    valueTable.value = `TBL_${name}`;
                    alarmTable.value = `TBL_TA_${name}`;
                }
                
                this.autoSelectCategory('instrument', name);
            });
        }

        // Controller form auto-fill
        const ctrlName = document.getElementById('ctrl_name');
        if (ctrlName) {
            ctrlName.addEventListener('input', (e) => {
                const name = e.target.value.toUpperCase();
                const valueTable = document.getElementById('ctrl_value_table');
                const alarmTable = document.getElementById('ctrl_alarm_table');
                
                if (name && valueTable && alarmTable) {
                    valueTable.value = `TBL_${name}`;
                    alarmTable.value = `TBL_TA_${name}`;
                }
                
                this.autoSelectCategory('controller', name);
            });
        }
    }

    autoSelectCategory(type, tagName) {
        if (type === 'instrument') {
            const categorySelect = document.getElementById('inst_category');
            const unitsSelect = document.getElementById('inst_units');
            
            if (tagName.startsWith('ET_')) {
                categorySelect.value = 'FLOW_TRANSMITTER';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('FIT_')) {
                categorySelect.value = 'FLOW_INDICATOR';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('TIT_')) {
                categorySelect.value = 'TEMPERATURE_INDICATOR';
                unitsSelect.value = '°C';
            } else if (tagName.startsWith('PIT_')) {
                categorySelect.value = 'PRESSURE_INDICATOR';
                unitsSelect.value = 'bar';
            } else if (tagName.startsWith('LIT_')) {
                categorySelect.value = 'LEVEL_INDICATOR';
                unitsSelect.value = 'm';
            } else if (tagName.startsWith('PDIT_')) {
                categorySelect.value = 'PRESSURE_DIFFERENTIAL';
                unitsSelect.value = 'bar';
            }
        } else if (type === 'controller') {
            const categorySelect = document.getElementById('ctrl_category');
            const unitsSelect = document.getElementById('ctrl_units');
            
            if (tagName.startsWith('FRC_')) {
                categorySelect.value = 'FLOW_CONTROLLER';
                unitsSelect.value = 'm3/h';
            } else if (tagName.startsWith('TRC_')) {
                categorySelect.value = 'TEMPERATURE_CONTROLLER';
                unitsSelect.value = '°C';
            } else if (tagName.startsWith('PRC_')) {
                categorySelect.value = 'PRESSURE_CONTROLLER';
                unitsSelect.value = 'bar';
            }
        }
    }

    setupFormValidation() {
        // Basic validation setup for both forms
        const instrumentForm = document.getElementById('instrumentForm');
        const controllerForm = document.getElementById('controllerForm');
        
        if (instrumentForm) {
            instrumentForm.addEventListener('input', () => this.validateInstrumentForm());
        }
        
        if (controllerForm) {
            controllerForm.addEventListener('input', () => this.validateControllerForm());
        }
    }

    resetInstrumentForm() {
        const form = document.getElementById('instrumentForm');
        if (form) {
            form.reset();
            this.editingTag = null;
            document.getElementById('instrumentModalTitle').textContent = 'Nuevo Instrumento de Medición';
        }
    }

    resetControllerForm() {
        const form = document.getElementById('controllerForm');
        if (form) {
            form.reset();
            this.editingTag = null;
            document.getElementById('controllerModalTitle').textContent = 'Nuevo Controlador PID';
            // Set default PID values
            document.getElementById('ctrl_kp').value = '1.0';
            document.getElementById('ctrl_ki').value = '0.1';
            document.getElementById('ctrl_kd').value = '0.01';
            document.getElementById('ctrl_output_low').value = '0';
            document.getElementById('ctrl_output_high').value = '100';
            document.getElementById('ctrl_pid_enable').checked = true;
        }
    }

    validateInstrumentForm() {
        const name = document.getElementById('inst_name').value;
        const category = document.getElementById('inst_category').value;
        const units = document.getElementById('inst_units').value;
        const valueTable = document.getElementById('inst_value_table').value;
        
        const isValid = name && category && units && valueTable;
        const saveBtn = document.getElementById('saveInstrument');
        if (saveBtn) {
            saveBtn.disabled = !isValid;
        }
        
        return isValid;
    }

    validateControllerForm() {
        const name = document.getElementById('ctrl_name').value;
        const category = document.getElementById('ctrl_category').value;
        const units = document.getElementById('ctrl_units').value;
        const valueTable = document.getElementById('ctrl_value_table').value;
        
        const isValid = name && category && units && valueTable;
        const saveBtn = document.getElementById('saveController');
        if (saveBtn) {
            saveBtn.disabled = !isValid;
        }
        
        return isValid;
    }

    async saveInstrument() {
        if (!this.validateInstrumentForm()) {
            this.showError('Por favor completa todos los campos requeridos');
            return;
        }

        try {
            const instrumentData = {
                name: document.getElementById('inst_name').value,
                opcua_name: document.getElementById('inst_name').value,
                value_table: document.getElementById('inst_value_table').value,
                alarm_table: document.getElementById('inst_alarm_table').value || '',
                description: document.getElementById('inst_description').value,
                units: document.getElementById('inst_units').value,
                category: document.getElementById('inst_category').value,
                opcua_table_index: parseInt(document.getElementById('inst_opcua_index').value) || 0,
                variables: [
                    "PV", "SV", "SetHH", "SetH", "SetL", "SetLL", 
                    "Input", "percent", "min", "max", "SIM_Value"
                ]
            };

            // Add alarm settings if provided
            const alarmSettings = {};
            const setHH = document.getElementById('inst_set_hh').value;
            const setH = document.getElementById('inst_set_h').value;
            const setL = document.getElementById('inst_set_l').value;
            const setLL = document.getElementById('inst_set_ll').value;
            const min = document.getElementById('inst_min').value;
            const max = document.getElementById('inst_max').value;

            if (setHH) alarmSettings.SetHH = parseFloat(setHH);
            if (setH) alarmSettings.SetH = parseFloat(setH);
            if (setL) alarmSettings.SetL = parseFloat(setL);
            if (setLL) alarmSettings.SetLL = parseFloat(setLL);
            if (min) alarmSettings.min = parseFloat(min);
            if (max) alarmSettings.max = parseFloat(max);

            if (Object.keys(alarmSettings).length > 0) {
                instrumentData.alarm_settings = alarmSettings;
            }

            await this.saveTagData(instrumentData, 'instrument');
            
            // Close modal
            const modal = bootstrap.Modal.getInstance(document.getElementById('instrumentModal'));
            if (modal) modal.hide();
            
        } catch (error) {
            console.error('Error saving instrument:', error);
            this.showError('Error al guardar el instrumento: ' + error.message);
        }
    }

    async saveController() {
        if (!this.validateControllerForm()) {
            this.showError('Por favor completa todos los campos requeridos');
            return;
        }

        try {
            const controllerData = {
                name: document.getElementById('ctrl_name').value,
                opcua_name: document.getElementById('ctrl_name').value,
                value_table: document.getElementById('ctrl_value_table').value,
                alarm_table: document.getElementById('ctrl_alarm_table').value || '',
                description: document.getElementById('ctrl_description').value,
                units: document.getElementById('ctrl_units').value,
                category: document.getElementById('ctrl_category').value,
                opcua_table_index: parseInt(document.getElementById('ctrl_opcua_index').value) || 0,
                variables: [
                    "PV", "SP", "CV", "KP", "KI", "KD", 
                    "auto_manual", "OUTPUT_HIGH", "OUTPUT_LOW", "PID_ENABLE"
                ]
            };

            // Add PID settings
            const pidSettings = {
                KP: parseFloat(document.getElementById('ctrl_kp').value) || 1.0,
                KI: parseFloat(document.getElementById('ctrl_ki').value) || 0.1,
                KD: parseFloat(document.getElementById('ctrl_kd').value) || 0.01,
                OUTPUT_LOW: parseInt(document.getElementById('ctrl_output_low').value) || 0,
                OUTPUT_HIGH: parseInt(document.getElementById('ctrl_output_high').value) || 100,
                PID_ENABLE: document.getElementById('ctrl_pid_enable').checked,
                auto_manual: document.getElementById('ctrl_auto_manual').checked
            };

            controllerData.pid_settings = pidSettings;

            await this.saveTagData(controllerData, 'controller');
            
            // Close modal
            const modal = bootstrap.Modal.getInstance(document.getElementById('controllerModal'));
            if (modal) modal.hide();
            
        } catch (error) {
            console.error('Error saving controller:', error);
            this.showError('Error al guardar el controlador: ' + error.message);
        }
    }

    async saveTagData(tagData, tagType) {
        const response = await scadaAPI.createTag(tagData, tagType);
        
        if (response.success) {
            this.showSuccess(`${tagType === 'instrument' ? 'Instrumento' : 'Controlador'} guardado correctamente`);
            await this.loadTags(); // Reload the tags list
        } else {
            throw new Error(response.message || 'Error desconocido');
        }
    }

    async loadTags() {
        console.log('🔄 Loading tags...');
        try {
            this.showLoading(true);
            
            console.log('📡 Fetching tags from API...');
            const response = await scadaAPI.getAllTags();
            console.log('📦 API Response:', response);
            
            // API returns 'data' not 'tags'
            const rawTags = response.data || [];
            
            // Group individual tags by parent tag name
            this.tags = this.groupTagsByParent(rawTags);
            this.filteredTags = [...this.tags];
            
            console.log(`✅ Loaded ${rawTags.length} individual tags, grouped into ${this.tags.length} parent tags`);
            
            this.renderTagsTable();
            this.showSuccess(`Cargados ${this.tags.length} tags principales con ${rawTags.length} variables`);
            
        } catch (error) {
            console.error('❌ Error loading tags:', error);
            this.showError('Error cargando tags: ' + scadaAPI.formatError(error));
            this.tags = [];
            this.filteredTags = [];
            this.renderTagsTable();
        } finally {
            this.showLoading(false);
        }
    }

    groupTagsByParent(rawTags) {
        const parentTags = new Map();
        
        rawTags.forEach(tag => {
            // Extract parent tag name (everything before the last dot, if any)
            const tagName = tag.name;
            let parentName;
            let variableName;
            
            // Check if this is a variable of a parent tag (contains a dot)
            if (tagName.includes('.')) {
                const lastDotIndex = tagName.lastIndexOf('.');
                parentName = tagName.substring(0, lastDotIndex);
                variableName = tagName.substring(lastDotIndex + 1);
            } else {
                // This is a parent tag itself
                parentName = tagName;
                variableName = null;
            }
            
            // Create or get parent tag entry
            if (!parentTags.has(parentName)) {
                parentTags.set(parentName, {
                    name: parentName,
                    description: this.getParentDescription(parentName),
                    category: this.getParentCategory(parentName),
                    type: this.getParentType(parentName),
                    variables: [],
                    variable_count: 0,
                    alarm_count: 0,
                    quality: 'Good',
                    last_update: tag.last_update || Date.now() / 1000,
                    is_expanded: false,
                    units: this.getParentUnits(parentName)
                });
            }
            
            const parentTag = parentTags.get(parentName);
            
            if (variableName) {
                // This is a variable, add it to the parent
                parentTag.variables.push({
                    name: variableName,
                    full_name: tagName,
                    description: tag.description,
                    units: tag.units,
                    category: tag.category,
                    quality: tag.quality || 'Good',
                    last_update: tag.last_update,
                    is_alarm: variableName.startsWith('ALARM_') || variableName.includes('ALARM'),
                    value: '-'  // Will be populated by real-time updates
                });
                
                parentTag.variable_count++;
                if (variableName.startsWith('ALARM_') || variableName.includes('ALARM')) {
                    parentTag.alarm_count++;
                }
            } else {
                // This is the parent tag itself, update its info
                if (tag.description) parentTag.description = tag.description;
                if (tag.category) parentTag.category = tag.category;
                if (tag.units) parentTag.units = tag.units;
            }
        });
        
        return Array.from(parentTags.values()).sort((a, b) => a.name.localeCompare(b.name));
    }
    
    getParentDescription(parentName) {
        // Infer description based on tag prefix
        const tagPrefixes = {
            'ET_': 'Flow Transmitter',
            'PIT_': 'Pressure Indicator Transmitter', 
            'TIT_': 'Temperature Indicator Transmitter',
            'FIT_': 'Flow Indicator Transmitter',
            'LIT_': 'Level Indicator Transmitter',
            'PDIT_': 'Differential Pressure Indicator Transmitter',
            'PRC_': 'Pressure Rate Controller',
            'TRC_': 'Temperature Rate Controller', 
            'FRC_': 'Flow Rate Controller'
        };
        
        for (const [prefix, description] of Object.entries(tagPrefixes)) {
            if (parentName.startsWith(prefix)) {
                return `${description} ${parentName}`;
            }
        }
        
        return `Industrial Tag ${parentName}`;
    }
    
    getParentCategory(parentName) {
        if (parentName.startsWith('ET_') || parentName.startsWith('PIT_') || 
            parentName.startsWith('TIT_') || parentName.startsWith('FIT_') ||
            parentName.startsWith('LIT_') || parentName.startsWith('PDIT_')) {
            return 'Instrumentos';
        } else if (parentName.startsWith('PRC_') || parentName.startsWith('TRC_') || 
                   parentName.startsWith('FRC_')) {
            return 'ControladorsPID';
        }
        return 'Otros';
    }
    
    getParentType(parentName) {
        if (parentName.startsWith('PRC_') || parentName.startsWith('TRC_') || 
            parentName.startsWith('FRC_')) {
            return 'controller';
        }
        return 'instrument';
    }
    
    getParentUnits(parentName) {
        const unitMappings = {
            'ET_': 'SCFH',
            'PIT_': 'PSI',
            'TIT_': '°F',
            'FIT_': 'SCFH', 
            'LIT_': '%',
            'PDIT_': 'PSI',
            'PRC_': 'PSI',
            'TRC_': '°F',
            'FRC_': 'SCFH'
        };
        
        for (const [prefix, unit] of Object.entries(unitMappings)) {
            if (parentName.startsWith(prefix)) {
                return unit;
            }
        }
        
        return '';
    }

    filterTags() {
        const searchTerm = document.getElementById('tag-search')?.value.toLowerCase() || '';
        const typeFilter = document.getElementById('tag-type-filter')?.value || '';
        const statusFilter = document.getElementById('tag-status-filter')?.value || '';

        this.filteredTags = this.tags.filter(tag => {
            const matchesSearch = !searchTerm || 
                tag.name.toLowerCase().includes(searchTerm) ||
                tag.description?.toLowerCase().includes(searchTerm) || '';
            
            const matchesType = !typeFilter || tag.type === typeFilter;
            
            const matchesStatus = !statusFilter || 
                (statusFilter === 'active' && tag.quality === 'Good') ||
                (statusFilter === 'inactive' && tag.quality !== 'Good');

            return matchesSearch && matchesType && matchesStatus;
        });

        this.renderTagsTable();
    }

    renderTagsTable() {
        console.log('🎨 Rendering hierarchical tags table...');
        const tbody = document.getElementById('tags-table-body');
        if (!tbody) {
            console.warn('⚠️  Table body element "tags-table-body" not found!');
            return;
        }

        console.log(`📊 Rendering ${this.filteredTags.length} parent tags`);

        if (this.filteredTags.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="7" class="text-center text-muted py-4">
                        <i class="fas fa-inbox fa-2x mb-2 d-block"></i>
                        No hay tags disponibles
                    </td>
                </tr>
            `;
            console.log('📝 Rendered empty state');
            return;
        }

        let html = '';
        this.filteredTags.forEach(tag => {
            html += this.renderParentTagRow(tag);
            if (tag.is_expanded) {
                html += this.renderVariableRows(tag);
            }
        });

        tbody.innerHTML = html;
        console.log(`✅ Rendered ${this.filteredTags.length} parent tags with expandable variables`);
    }

    renderParentTagRow(tag) {
        const lastUpdate = new Date(tag.last_update * 1000).toLocaleString('es-ES');
        const typeBadge = this.getTagTypeBadge(tag.type);
        const categoryBadge = this.getCategoryBadge(tag.category);
        const statusBadge = this.getQualityBadge(tag.quality);
        const expandIcon = tag.is_expanded ? 'fa-chevron-down' : 'fa-chevron-right';
        
        return `
            <tr class="parent-tag-row table-primary" data-tag-name="${tag.name}">
                <td>
                    <div class="d-flex align-items-center">
                        <i class="fas ${expandIcon} me-2 expand-toggle" 
                           style="cursor: pointer; color: #0d6efd;" 
                           onclick="tagsManager.toggleTagExpansion('${tag.name}')"></i>
                        <div>
                            <strong>${tag.name}</strong>
                            ${tag.units ? `<small class="text-muted d-block">${tag.units}</small>` : ''}
                            <small class="text-info">${tag.variable_count} variables, ${tag.alarm_count} alarmas</small>
                        </div>
                    </div>
                </td>
                <td>
                    ${typeBadge}
                    ${categoryBadge}
                </td>
                <td>
                    <span class="fw-bold text-primary">TAG PADRE</span>
                    <small class="text-muted d-block">${tag.variable_count} variables</small>
                </td>
                <td>
                    ${statusBadge}
                </td>
                <td>
                    ${tag.description || '-'}
                </td>
                <td>
                    <small>${lastUpdate}</small>
                </td>
                <td>
                    <div class="btn-group btn-group-sm" role="group">
                        <button type="button" class="btn btn-outline-primary" onclick="tagsManager.editParentTag('${tag.name}')" title="Editar Tag">
                            <i class="fas fa-edit"></i>
                        </button>
                        <button type="button" class="btn btn-outline-info" onclick="tagsManager.viewTagDetails('${tag.name}')" title="Ver Detalles">
                            <i class="fas fa-eye"></i>
                        </button>
                        <button type="button" class="btn btn-outline-danger" onclick="tagsManager.deleteParentTag('${tag.name}')" title="⚠️ Eliminar Tag Completo">
                            <i class="fas fa-trash"></i>
                        </button>
                    </div>
                </td>
            </tr>
        `;
    }

    renderVariableRows(tag) {
        let html = '';
        tag.variables.forEach(variable => {
            const lastUpdate = new Date(variable.last_update * 1000).toLocaleString('es-ES');
            const statusBadge = this.getQualityBadge(variable.quality);
            const alarmIcon = variable.is_alarm ? '<i class="fas fa-exclamation-triangle text-warning me-1"></i>' : '';
            
            html += `
                <tr class="variable-row table-light" data-parent-tag="${tag.name}" data-variable-name="${variable.name}">
                    <td style="padding-left: 40px;">
                        <div class="d-flex align-items-center">
                            <i class="fas fa-arrow-right me-2 text-muted" style="font-size: 0.8em;"></i>
                            <div>
                                ${alarmIcon}<code>${variable.name}</code>
                                ${variable.units ? `<small class="text-muted d-block">${variable.units}</small>` : ''}
                            </div>
                        </div>
                    </td>
                    <td>
                        <span class="badge ${variable.is_alarm ? 'bg-warning text-dark' : 'bg-info'}">
                            ${variable.is_alarm ? 'ALARM' : 'Variable'}
                        </span>
                    </td>
                    <td>
                        <span class="fw-bold">${variable.value}</span>
                        <small class="text-muted d-block">Parte de ${tag.name}</small>
                    </td>
                    <td>
                        ${statusBadge}
                    </td>
                    <td>
                        ${variable.description || `Variable ${variable.name} del tag ${tag.name}`}
                    </td>
                    <td>
                        <small>${lastUpdate}</small>
                    </td>
                    <td>
                        <div class="btn-group btn-group-sm" role="group">
                            <button type="button" class="btn btn-outline-secondary btn-sm" 
                                    onclick="tagsManager.editVariable('${tag.name}', '${variable.name}')" 
                                    title="Editar Variable">
                                <i class="fas fa-edit" style="font-size: 0.7em;"></i>
                            </button>
                            <button type="button" class="btn btn-outline-warning btn-sm" 
                                    onclick="tagsManager.showVariableDeleteWarning('${tag.name}', '${variable.name}')" 
                                    title="⚠️ CUIDADO: Eliminar variable puede romper el tag">
                                <i class="fas fa-exclamation-triangle" style="font-size: 0.7em;"></i>
                            </button>
                        </div>
                    </td>
                </tr>
            `;
        });
        return html;
    }

    getTagTypeBadge(type) {
        const badges = {
            'instrument': '<span class="badge bg-primary">Instrumento</span>',
            'controller': '<span class="badge bg-success">Controlador</span>',
            'float': '<span class="badge bg-info">Float</span>',
            'bool': '<span class="badge bg-success">Boolean</span>',
            'int': '<span class="badge bg-primary">Integer</span>',
            'string': '<span class="badge bg-secondary">String</span>'
        };
        return badges[type] || `<span class="badge bg-secondary">${type}</span>`;
    }
    
    getCategoryBadge(category) {
        const badges = {
            'Instrumentos': '<span class="badge bg-outline-primary ms-1">📊 Instrumentos</span>',
            'ControladorsPID': '<span class="badge bg-outline-success ms-1">🎛️ Controladores</span>',
            'Otros': '<span class="badge bg-outline-secondary ms-1">Otros</span>'
        };
        return badges[category] || `<span class="badge bg-outline-secondary ms-1">${category || 'Sin categoría'}</span>`;
    }

    toggleTagExpansion(tagName) {
        const tag = this.filteredTags.find(t => t.name === tagName);
        if (tag) {
            tag.is_expanded = !tag.is_expanded;
            this.renderTagsTable();
            console.log(`🔄 Toggled expansion for tag: ${tagName} (expanded: ${tag.is_expanded})`);
        }
    }

    getQualityBadge(quality) {
        const badges = {
            'Good': '<span class="badge bg-success">Buena</span>',
            'Bad': '<span class="badge bg-danger">Mala</span>',
            'Uncertain': '<span class="badge bg-warning">Incierta</span>',
            'Unknown': '<span class="badge bg-secondary">Desconocida</span>'
        };
        return badges[quality] || badges['Unknown'];
    }

    getTypeBadge(type) {
        const badges = {
            'float': '<span class="badge bg-info">Float</span>',
            'bool': '<span class="badge bg-success">Boolean</span>',
            'int': '<span class="badge bg-primary">Integer</span>',
            'string': '<span class="badge bg-secondary">String</span>'
        };
        return badges[type] || `<span class="badge bg-secondary">${type}</span>`;
    }

    formatValue(value, type) {
        if (value === null || value === undefined) return '-';
        
        switch (type) {
            case 'float':
                return parseFloat(value).toFixed(2);
            case 'bool':
                return value ? 'True' : 'False';
            case 'int':
                return parseInt(value).toString();
            default:
                return value.toString();
        }
    }

    async editTag(tagName) {
        console.log(`🔧 Editing tag: ${tagName}`);
        try {
            console.log(`📡 Fetching tag data for: ${tagName}`);
            const response = await scadaAPI.getTag(tagName);
            console.log(`📦 Received tag data:`, response);
            
            // Extract tag data from response
            const tag = response.data || response;
            this.editingTag = tag;
            
            // Determine tag type from name
            const tagType = this.detectTagType(tag.name);
            console.log(`�️  Detected tag type: ${tagType} for ${tag.name}`);
            
            if (tagType === 'controller') {
                // Open controller modal
                const modalElement = document.getElementById('controllerModal');
                const titleElement = document.getElementById('controllerModalLabel');
                
                if (!modalElement) {
                    throw new Error('Controller modal not found');
                }
                
                if (titleElement) {
                    titleElement.textContent = `Editar Controlador: ${tag.name}`;
                }
                
                // TODO: Populate controller modal with tag data
                // this.populateControllerModal(tag);
                
                const modal = new bootstrap.Modal(modalElement);
                modal.show();
                
                console.log('✅ Controller edit modal opened');
                
            } else {
                // Open instrument modal
                const modalElement = document.getElementById('instrumentModal');
                const titleElement = document.getElementById('instrumentModalLabel');
                
                if (!modalElement) {
                    throw new Error('Instrument modal not found');
                }
                
                if (titleElement) {
                    titleElement.textContent = `Editar Instrumento: ${tag.name}`;
                }
                
                // TODO: Populate instrument modal with tag data
                // this.populateInstrumentModal(tag);
                
                const modal = new bootstrap.Modal(modalElement);
                modal.show();
                
                console.log('✅ Instrument edit modal opened');
            }
            
        } catch (error) {
            console.error('❌ Error in editTag:', error);
            this.showError('Error cargando tag: ' + error.message);
        }
    }

    async viewTagDetails(tagName) {
        try {
            const tag = await scadaAPI.getTag(tagName);
            this.showTagDetails(tag);
            
        } catch (error) {
            this.showError('Error cargando detalles del tag: ' + scadaAPI.formatError(error));
        }
    }

    showTagDetails(tag) {
        const details = `
            <div class="modal fade" id="tagDetailsModal" tabindex="-1">
                <div class="modal-dialog modal-lg">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">Detalles del Tag: ${tag.name}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <div class="row">
                                <div class="col-md-6">
                                    <h6>Información General</h6>
                                    <table class="table table-sm">
                                        <tr><td><strong>Nombre:</strong></td><td>${tag.name}</td></tr>
                                        <tr><td><strong>Tipo:</strong></td><td>${tag.type}</td></tr>
                                        <tr><td><strong>Valor:</strong></td><td>${this.formatValue(tag.value, tag.type)}</td></tr>
                                        <tr><td><strong>Calidad:</strong></td><td>${this.getQualityBadge(tag.quality)}</td></tr>
                                        <tr><td><strong>Unidad:</strong></td><td>${tag.unit || '-'}</td></tr>
                                        <tr><td><strong>Descripción:</strong></td><td>${tag.description || '-'}</td></tr>
                                    </table>
                                </div>
                                <div class="col-md-6">
                                    <h6>Configuración PAC</h6>
                                    <table class="table table-sm">
                                        <tr><td><strong>Dirección PAC:</strong></td><td>${tag.pac_address || '-'}</td></tr>
                                        <tr><td><strong>Valor Mín:</strong></td><td>${tag.limits?.min || '-'}</td></tr>
                                        <tr><td><strong>Valor Máx:</strong></td><td>${tag.limits?.max || '-'}</td></tr>
                                        <tr><td><strong>Última Actualización:</strong></td><td>${tag.last_update ? new Date(tag.last_update).toLocaleString('es-ES') : '-'}</td></tr>
                                    </table>
                                    
                                    ${tag.history && tag.history.length > 0 ? `
                                        <h6>Historial Reciente</h6>
                                        <div style="max-height: 150px; overflow-y: auto;">
                                            <table class="table table-sm">
                                                <thead>
                                                    <tr><th>Tiempo</th><th>Valor</th></tr>
                                                </thead>
                                                <tbody>
                                                    ${tag.history.slice(-10).map(entry => `
                                                        <tr>
                                                            <td><small>${new Date(entry.timestamp).toLocaleString('es-ES')}</small></td>
                                                            <td>${this.formatValue(entry.value, tag.type)}</td>
                                                        </tr>
                                                    `).join('')}
                                                </tbody>
                                            </table>
                                        </div>
                                    ` : ''}
                                </div>
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cerrar</button>
                        </div>
                    </div>
                </div>
            </div>
        `;

        // Remove existing modal if any
        const existingModal = document.getElementById('tagDetailsModal');
        if (existingModal) {
            existingModal.remove();
        }

        // Add new modal to body
        document.body.insertAdjacentHTML('beforeend', details);
        
        // Show modal
        const modal = new bootstrap.Modal(document.getElementById('tagDetailsModal'));
        modal.show();

        // Clean up after hide
        modal._element.addEventListener('hidden.bs.modal', () => {
            modal._element.remove();
        });
    }

    populateModal(tag) {
        document.getElementById('tagName').value = tag.name || '';
        document.getElementById('tagType').value = tag.type || '';
        document.getElementById('tagAddress').value = tag.pac_address || '';
        document.getElementById('tagUnit').value = tag.unit || '';
        document.getElementById('tagDescription').value = tag.description || '';
        document.getElementById('tagMinValue').value = tag.limits?.min || '';
        document.getElementById('tagMaxValue').value = tag.limits?.max || '';
    }

    clearModal() {
        document.getElementById('tagForm').reset();
        this.editingTag = null;
        document.getElementById('tagModalTitle').textContent = 'Nuevo Tag';
    }

    validateForm() {
        const name = document.getElementById('tagName').value.trim();
        const type = document.getElementById('tagType').value;
        const saveBtn = document.getElementById('saveTag');
        
        const isValid = name && type;
        saveBtn.disabled = !isValid;
        
        return isValid;
    }

    async saveTag() {
        if (!this.validateForm()) return;

        const tagData = {
            name: document.getElementById('tagName').value.trim(),
            type: document.getElementById('tagType').value,
            pac_address: document.getElementById('tagAddress').value.trim() || null,
            unit: document.getElementById('tagUnit').value.trim() || null,
            description: document.getElementById('tagDescription').value.trim() || null,
            limits: {}
        };

        const minValue = document.getElementById('tagMinValue').value;
        const maxValue = document.getElementById('tagMaxValue').value;
        
        if (minValue) tagData.limits.min = parseFloat(minValue);
        if (maxValue) tagData.limits.max = parseFloat(maxValue);
        
        if (Object.keys(tagData.limits).length === 0) {
            delete tagData.limits;
        }

        try {
            this.showLoading(true);
            
            if (this.editingTag) {
                await scadaAPI.updateTag(this.editingTag.name, tagData);
                this.showSuccess('Tag actualizado correctamente');
            } else {
                await scadaAPI.createTag(tagData);
                this.showSuccess('Tag creado correctamente');
            }

            const modal = bootstrap.Modal.getInstance(document.getElementById('tagModal'));
            modal.hide();
            
            this.clearModal();
            await this.loadTags();
            
        } catch (error) {
            this.showError('Error guardando tag: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    async deleteTag(tagName) {
        // Legacy function - redirect to parent tag delete with warning
        this.showVariableDeleteWarning('', tagName);
    }

    async deleteParentTag(tagName) {
        const tag = this.tags.find(t => t.name === tagName);
        if (!tag) {
            this.showError('Tag no encontrado');
            return;
        }

        const confirmMessage = `⚠️ ELIMINAR TAG COMPLETO: "${tagName}"

Esta acción eliminará:
- El tag padre: ${tagName}
- ${tag.variable_count} variables asociadas
- ${tag.alarm_count} configuraciones de alarma
- Toda la configuración OPC UA

Esta operación NO SE PUEDE DESHACER.

¿Está seguro de que desea continuar?`;

        if (!confirm(confirmMessage)) {
            return;
        }

        // Additional confirmation for critical tags
        if (tag.variable_count > 5) {
            const doubleConfirm = prompt(`Para confirmar, escriba exactamente: ${tagName}`);
            if (doubleConfirm !== tagName) {
                this.showError('Confirmación incorrecta. Operación cancelada.');
                return;
            }
        }

        try {
            this.showLoading(true);
            
            // Delete all variables first
            for (const variable of tag.variables) {
                await scadaAPI.deleteTag(variable.full_name);
            }
            
            // Then delete parent tag if it exists as individual tag
            await scadaAPI.deleteTag(tagName);
            
            this.showSuccess(`Tag completo "${tagName}" eliminado correctamente`);
            await this.loadTags();
            
        } catch (error) {
            this.showError('Error eliminando tag: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    showVariableDeleteWarning(parentTagName, variableName) {
        const warningMessage = `⚠️ ADVERTENCIA: ELIMINACIÓN DE VARIABLE

Está a punto de eliminar la variable: "${variableName}"
${parentTagName ? `Del tag padre: "${parentTagName}"` : ''}

🚨 RIESGOS:
- Puede romper la funcionalidad del tag padre
- Puede causar errores en OPC UA
- Puede afectar la comunicación con PAC Control
- Los clientes OPC UA pueden dejar de funcionar correctamente

🔧 ALTERNATIVAS RECOMENDADAS:
- Editar la variable en lugar de eliminarla
- Deshabilitar la variable temporalmente
- Eliminar el tag completo si es necesario

¿Realmente desea eliminar esta variable individual?
(Esto NO es recomendado para operación normal)`;

        if (confirm(warningMessage)) {
            const finalConfirm = confirm('ÚLTIMA CONFIRMACIÓN: ¿Eliminar la variable sabiendo los riesgos?');
            if (finalConfirm) {
                this.deleteVariable(parentTagName, variableName);
            }
        }
    }

    async deleteVariable(parentTagName, variableName) {
        const fullVariableName = parentTagName ? `${parentTagName}.${variableName}` : variableName;
        
        try {
            this.showLoading(true);
            
            await scadaAPI.deleteTag(fullVariableName);
            this.showSuccess(`Variable "${variableName}" eliminada (con riesgos)`);
            await this.loadTags();
            
        } catch (error) {
            this.showError('Error eliminando variable: ' + scadaAPI.formatError(error));
        } finally {
            this.showLoading(false);
        }
    }

    editParentTag(tagName) {
        const tag = this.tags.find(t => t.name === tagName);
        if (!tag) {
            this.showError('Tag no encontrado');
            return;
        }

        // For now, show tag details or redirect to appropriate edit modal
        this.viewTagDetails(tagName);
    }

    viewTagDetails(tagName) {
        const tag = this.tags.find(t => t.name === tagName);
        if (!tag) {
            this.showError('Tag no encontrado');
            return;
        }

        let detailsHTML = `
            <div class="modal fade" id="tagDetailsModal" tabindex="-1">
                <div class="modal-dialog modal-lg">
                    <div class="modal-content">
                        <div class="modal-header">
                            <h5 class="modal-title">
                                <i class="fas fa-info-circle me-2"></i>
                                Detalles del Tag: ${tag.name}
                            </h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                        </div>
                        <div class="modal-body">
                            <div class="row mb-3">
                                <div class="col-md-6">
                                    <strong>Nombre:</strong> ${tag.name}<br>
                                    <strong>Tipo:</strong> ${tag.type}<br>
                                    <strong>Categoría:</strong> ${tag.category}<br>
                                    <strong>Unidades:</strong> ${tag.units || 'N/A'}
                                </div>
                                <div class="col-md-6">
                                    <strong>Variables:</strong> ${tag.variable_count}<br>
                                    <strong>Alarmas:</strong> ${tag.alarm_count}<br>
                                    <strong>Estado:</strong> ${tag.quality}<br>
                                    <strong>Actualización:</strong> ${new Date(tag.last_update * 1000).toLocaleString()}
                                </div>
                            </div>
                            
                            <h6>Variables del Tag:</h6>
                            <div class="table-responsive">
                                <table class="table table-sm table-striped">
                                    <thead>
                                        <tr>
                                            <th>Variable</th>
                                            <th>Tipo</th>
                                            <th>Valor</th>
                                            <th>Estado</th>
                                        </tr>
                                    </thead>
                                    <tbody>
        `;

        tag.variables.forEach(variable => {
            detailsHTML += `
                <tr>
                    <td>
                        ${variable.is_alarm ? '<i class="fas fa-exclamation-triangle text-warning me-1"></i>' : ''}
                        <code>${variable.name}</code>
                    </td>
                    <td>
                        <span class="badge ${variable.is_alarm ? 'bg-warning text-dark' : 'bg-info'}">
                            ${variable.is_alarm ? 'ALARM' : 'Variable'}
                        </span>
                    </td>
                    <td>${variable.value}</td>
                    <td>${this.getQualityBadge(variable.quality)}</td>
                </tr>
            `;
        });

        detailsHTML += `
                                    </tbody>
                                </table>
                            </div>
                        </div>
                        <div class="modal-footer">
                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cerrar</button>
                            <button type="button" class="btn btn-primary" onclick="tagsManager.editParentTagForm('${tag.name}')">
                                <i class="fas fa-edit me-1"></i>Editar Tag
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        `;

        // Remove existing modal and add new one
        const existingModal = document.getElementById('tagDetailsModal');
        if (existingModal) {
            existingModal.remove();
        }

        document.body.insertAdjacentHTML('beforeend', detailsHTML);
        
        // Show modal
        const modal = new bootstrap.Modal(document.getElementById('tagDetailsModal'));
        modal.show();
    }

    editVariable(parentTagName, variableName) {
        this.showError(`Edición de variables individuales no implementada aún. Use "Ver Detalles" del tag padre para ver información completa.`);
    }

    editParentTagForm(tagName) {
        // Close details modal first
        const detailsModal = bootstrap.Modal.getInstance(document.getElementById('tagDetailsModal'));
        if (detailsModal) {
            detailsModal.hide();
        }

        // Show appropriate edit modal based on tag type
        const tag = this.tags.find(t => t.name === tagName);
        if (tag.type === 'controller') {
            // Show controller modal
            this.showError('Edición de controladores no implementada aún');
        } else {
            // Show instrument modal  
            this.showError('Edición de instrumentos no implementada aún');
        }
    }

    showLoading(show) {
        const spinner = document.getElementById('loading-spinner');
        if (spinner) {
            spinner.classList.toggle('d-none', !show);
        }
    }

    showSuccess(message) {
        this.showToast(message, 'success');
    }

    showError(message) {
        this.showToast(message, 'danger');
    }

    showToast(message, type = 'info') {
        // Simple toast implementation - could use Bootstrap Toast for better UX
        const alertClass = type === 'success' ? 'alert-success' : 
                          type === 'danger' ? 'alert-danger' : 'alert-info';
        
        const toast = document.createElement('div');
        toast.className = `alert ${alertClass} alert-dismissible position-fixed top-0 end-0 m-3`;
        toast.style.zIndex = '9999';
        toast.innerHTML = `
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        // Auto-remove after 5 seconds
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    }
}

// Global tags manager instance
window.tagsManager = new TagsManager();